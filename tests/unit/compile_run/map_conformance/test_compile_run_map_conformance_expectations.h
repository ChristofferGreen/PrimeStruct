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
    // TODO-4756/TODO-4757 (fixed): Result.why() on a ContainerError
    // produced by a map tryAt miss now reports the real message instead of
    // an empty string.
    const std::string expectedOutput = "container missing key\n";
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == expectedOutput);
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
  CHECK(readFile(outPath) == "\n");
}

inline void expectExperimentalMapMethodConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  expectMapConformanceCompileReject(makeExperimentalMapMethodConformanceSource(),
                                    "experimental_map_methods",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectExperimentalMapReferenceHelperConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapReferenceHelperConformanceSource();
  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(source,
                                    "experimental_map_reference_helpers_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectPublicMapReferenceWrapperConformance(const std::string &emitMode) {
  const std::string source = makePublicMapReferenceWrapperConformanceSource();
  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(source,
                                    "public_map_reference_wrappers_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectExperimentalMapReferenceMethodConformance(const std::string &emitMode) {
  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(makeExperimentalMapReferenceMethodConformanceSource(),
                                    "experimental_map_reference_methods",
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectExperimentalMapVariadicConstructorConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorConformanceSource(),
                                      "experimental_map_variadic_ctor_" + emitMode,
                                      emitMode,
                                      "");
    return;
  }
  if (emitMode == "vm") {
    // TODO-4741: experimental Map<K,V> variadic constructors are unimplemented.
    expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorConformanceSource(),
                                      "experimental_map_variadic_ctor_" + emitMode,
                                      emitMode,
                                      "argument count mismatch for /std/collections/map/map__ov1");
    return;
  }
  expectMapConformanceProgramRuns(makeExperimentalMapVariadicConstructorConformanceSource(),
                                  "experimental_map_variadic_ctor_" + emitMode,
                                  emitMode,
                                  18);
}

inline void expectExperimentalMapVariadicConstructorMismatchReject(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorMismatchSource(),
                                      "experimental_map_variadic_ctor_mismatch",
                                      emitMode,
                                      "");
    return;
  }

  if (emitMode == "vm") {
    // TODO-4741: experimental Map<K,V> variadic constructors are unimplemented;
    // this now fails earlier during lowering instead of at the type-mismatch check.
    expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorMismatchSource(),
                                      "experimental_map_variadic_ctor_mismatch",
                                      emitMode,
                                      "vm backend only supports indexing into string literals or string bindings");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapVariadicConstructorMismatchSource(),
                                    "experimental_map_variadic_ctor_mismatch",
                                    emitMode,
                                    "argument type mismatch");
}

inline void expectExperimentalMapInsertConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapInsertConformanceSource(),
                                    "experimental_map_insert",
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectExperimentalMapOwnershipConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    // TODO-4741: experimental Map<K,V> constructors (mapSingle) are unimplemented.
    expectMapConformanceCompileReject(makeExperimentalMapOwnershipConformanceSource(),
                                      "experimental_map_ownership",
                                      emitMode,
                                      "unknown call target: mapSingle");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapOwnershipConformanceSource(),
                                    "experimental_map_ownership",
                                    emitMode,
                                    "");
}

inline void expectCanonicalMapNamespaceExperimentalInsertConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    // TODO-4741: experimental Map<K,V> constructors (mapSingle) are unimplemented.
    expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalInsertConformanceSource(),
                                      "map_namespace_canonical_experimental_insert",
                                      emitMode,
                                      "unknown call target: mapSingle");
    return;
  }

  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalInsertConformanceSource(),
                                    "map_namespace_canonical_experimental_insert",
                                    emitMode,
                                    "");
}

inline void expectBuiltinCanonicalMapInsertFirstGrowthConformance(const std::string &emitMode) {
  // TODO-4749 (fixed): the source was missing `import /std/collections/map/*`
  // - without it, `.at()`/`.at_unsafe()` method-call sugar on a `map<K,V>`
  // receiver correctly falls back to the "legacy alias receiver, helper not
  // imported" retirement diagnostic (still exercised deliberately by
  // "canonical map value methods report retired insert diagnostics" in
  // test_semantics_calls_and_flow_collections_count_helpers_and_bare_map_calls.cpp).
  // Adding the missing import lets it resolve to the real canonical helper.
  expectMapConformanceProgramRuns(makeBuiltinCanonicalMapInsertFirstGrowthConformanceSource(),
                                  "map_builtin_canonical_insert_first_growth_" + emitMode,
                                  emitMode,
                                  8);
}

inline void expectBuiltinCanonicalMapInsertRepeatedGrowthConformance(const std::string &emitMode) {
  // TODO-4749 (fixed): see expectBuiltinCanonicalMapInsertFirstGrowthConformance.
  expectMapConformanceProgramRuns(makeBuiltinCanonicalMapInsertRepeatedGrowthConformanceSource(),
                                  "map_builtin_canonical_insert_repeated_growth_" + emitMode,
                                  emitMode,
                                  197);
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

inline void expectBuiltinCanonicalMapInsertEleventhGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertEleventhGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_eleventh_growth_" + emitMode,
                                            emitMode,
                                            198,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertTwelfthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertTwelfthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_twelfth_growth_" + emitMode,
                                            emitMode,
                                            228,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertThirteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertThirteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_thirteenth_growth_" + emitMode,
                                            emitMode,
                                            253,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertFourteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertFourteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_fourteenth_growth_" + emitMode,
                                            emitMode,
                                            125,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertFifteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertFifteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_fifteenth_growth_" + emitMode,
                                            emitMode,
                                            132,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertSixteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertSixteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_sixteenth_growth_" + emitMode,
                                            emitMode,
                                            137,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertSeventeenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertSeventeenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_seventeenth_growth_" + emitMode,
                                            emitMode,
                                            142,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertEighteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertEighteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_eighteenth_growth_" + emitMode,
                                            emitMode,
                                            147,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertNineteenthGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertNineteenthGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_nineteenth_growth_" + emitMode,
                                            emitMode,
                                            152,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertTwentiethGrowthConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBuiltinCanonicalMapInsertTwentiethGrowthConformanceSource(),
                                            "map_builtin_canonical_insert_twentieth_growth_" + emitMode,
                                            emitMode,
                                            157,
                                            "");
}

inline void expectBuiltinCanonicalMapInsertOverwriteConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(makeBuiltinCanonicalMapInsertOverwriteConformanceSource(),
                                      "map_builtin_canonical_insert_overwrite_" + emitMode,
                                      emitMode,
                                      "unknown call target: /map/at [PSC1005]");
    return;
  }

  expectMapConformanceCompileReject(makeBuiltinCanonicalMapInsertOverwriteConformanceSource(),
                                    "map_builtin_canonical_insert_overwrite_" + emitMode,
                                    emitMode,
                                    "unknown call target: /map/at");
}

inline void expectBuiltinCanonicalMapInsertNonLocalGrowthConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeBuiltinCanonicalMapInsertNonLocalGrowthConformanceSource(),
      "map_builtin_canonical_insert_non_local_growth_" + emitMode,
      emitMode,
      "");
}

inline void expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeBuiltinCanonicalMapInsertNestedNonLocalGrowthConformanceSource(),
      "map_builtin_canonical_insert_nested_non_local_growth_" + emitMode,
      emitMode,
      "");
}

inline void expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformanceSource(),
      "map_builtin_canonical_insert_helper_return_borrowed_method_" + emitMode,
      emitMode,
      "");
}

inline void expectBuiltinCanonicalMapStructFieldInitializerConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeBuiltinCanonicalMapStructFieldInitializerConformanceSource(),
      "map_builtin_canonical_struct_field_initializer_" + emitMode,
      emitMode,
      "");
}

inline void expectBuiltinCanonicalMapInsertHelperReturnValueDirectConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(
      makeBuiltinCanonicalMapInsertHelperReturnValueDirectConformanceSource(),
      "map_builtin_canonical_insert_helper_return_value_direct_" + emitMode,
      emitMode,
      0,
      "");
}

inline void expectBuiltinCanonicalMapInsertHelperReturnValueMethodConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(
      makeBuiltinCanonicalMapInsertHelperReturnValueMethodConformanceSource(),
      "map_builtin_canonical_insert_helper_return_value_method_" + emitMode,
      emitMode,
      0,
      "");
}

inline void expectBuiltinCanonicalMapInsertBorrowedHolderFieldDirectConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeBuiltinCanonicalMapInsertBorrowedHolderFieldDirectConformanceSource(),
      "map_builtin_canonical_insert_borrowed_holder_field_direct_" + emitMode,
      emitMode,
      "");
}

inline void expectExperimentalMapIndexConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapIndexConformanceSource(),
                                    "experimental_map_index",
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectCanonicalMapNamespaceVmConformance() {
  // TODO-4756/TODO-4757 (fixed): Result.why() on a ContainerError produced
  // by a map tryAt miss now reports the real message instead of an empty
  // string.
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
  expectMapConformanceCompileReject(
      makeCanonicalMapNamespaceExperimentalReferenceConformanceSource(),
      "map_namespace_canonical_experimental_reference_" + emitMode,
      emitMode,
      "template arguments are only supported on templated definitions: /Map");
}
