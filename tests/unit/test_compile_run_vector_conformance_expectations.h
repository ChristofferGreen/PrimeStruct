#pragma once

inline void expectVectorConformanceCompileReject(const std::string &source,
                                                 const std::string &nameStem,
                                                 const std::string &emitMode,
                                                 const std::string &expectedFragment,
                                                 const std::string &requiredFragment = "");

inline void expectVectorConformanceProgramRuns(const std::string &source,
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

inline void expectExperimentalVectorCanonicalHelperRoutingConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceProgramRuns(makeExperimentalVectorCanonicalHelperRoutingSource(),
                                       "experimental_vector_canonical_helper_routing_" + emitMode,
                                       emitMode,
                                       93);
    return;
  }
  if (emitMode == "native" || emitMode == "exe") {
    expectVectorConformanceCompileReject(
        makeExperimentalVectorCanonicalHelperRoutingSource(),
        "experimental_vector_canonical_helper_routing_" + emitMode,
        emitMode,
        "unknown call target: /std/collections/experimental_vector/vectorPop");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeExperimentalVectorCanonicalHelperRoutingSource(),
      "experimental_vector_canonical_helper_routing_" + emitMode,
      emitMode,
      93);
}

inline void expectVectorVmProgramRunsWithOutput(const std::string &source,
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

inline void expectVectorConformanceCompileReject(const std::string &source,
                                                 const std::string &nameStem,
                                                 const std::string &emitMode,
                                                 const std::string &expectedFragment,
                                                 const std::string &requiredFragment) {
  CAPTURE(nameStem);
  CAPTURE(emitMode);
  CAPTURE(expectedFragment);
  CAPTURE(requiredFragment);
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  const std::string discardExePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_discard_exe")).string();
  const std::string command = emitMode == "vm"
                                  ? "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                                        quoteShellArg(outPath) + " 2>&1"
                                  : "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                        " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                        quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(command) == 2);
  const std::string diagnostics = readFile(outPath);
  if (!requiredFragment.empty()) {
    CHECK(diagnostics.find(requiredFragment) != std::string::npos);
  }
  CHECK(diagnostics.find(expectedFragment) != std::string::npos);
}

inline void expectVectorConformanceCompileCrash(const std::string &source,
                                                const std::string &nameStem,
                                                const std::string &emitMode) {
  CAPTURE(nameStem);
  CAPTURE(emitMode);
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  const std::string discardExePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_discard_exe")).string();
  const std::string command = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                              " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                              quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(command) == 139);
}

inline void expectVmStdlibVectorHelperSurfaceConformance() {
  expectVectorVmProgramRunsWithOutput(makeVectorHelperSurfaceConformanceSource("/std/collections/*"),
                                      "vector_helper_surface_stdlib",
                                      40,
                                      "0\n2\n2\n13\n11\n5\n7\n");
}

inline void expectVmStdlibVectorExtendedConstructorConformance() {
  expectVectorVmProgramRunsWithOutput(makeVectorExtendedConstructorConformanceSource("/std/collections/*"),
                                      "vector_extended_ctor_stdlib",
                                      32,
                                      "8\n8\n14\n2\n");
}

inline void expectVmStdlibVectorGrowthConformance() {
  expectVectorVmProgramRunsWithOutput(makeVectorGrowthConformanceSource("/std/collections/*"),
                                      "vector_growth_stdlib",
                                      71,
                                      "2\n3\n11\n22\n33\n");
}

inline void expectVmStdlibVectorShrinkRemoveConformance() {
  expectVectorVmProgramRunsWithOutput(makeVectorShrinkRemoveConformanceSource("/std/collections/*"),
                                      "vector_shrink_remove_stdlib",
                                      86,
                                      "3\n20\n2\n30\n1\n30\n0\n");
}

inline void expectVectorTypeMismatchReject(const std::string &emitMode,
                                           const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceCompileReject(makeVectorTypeMismatchRejectSource(importPath),
                                       "vector_type_mismatch_" + slug,
                                       emitMode,
                                       "/std/collections/vectorPair",
                                       "argument type mismatch");
}

inline void expectVectorPopTypeMismatchReject(const std::string &emitMode,
                                              const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceCompileReject(makeVectorPopTypeMismatchRejectSource(importPath),
                                       "vector_pop_type_mismatch_" + slug,
                                       emitMode,
                                       "/std/collections/vector/pop",
                                       "argument type mismatch");
}

inline void expectVectorPushTypeMismatchReject(const std::string &emitMode,
                                               const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceCompileReject(makeVectorPushTypeMismatchRejectSource(importPath),
                                       "vector_push_type_mismatch_" + slug,
                                       emitMode,
                                       "/std/collections/vector/push",
                                       "argument type mismatch");
}

inline void expectCanonicalVectorNamespaceConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceProgramRuns(
        makeCanonicalVectorNamespaceConformanceSource(),
        "vector_namespace_canonical_" + emitMode,
        emitMode,
        109);
    return;
  }

  expectVectorConformanceCompileReject(
      makeCanonicalVectorNamespaceConformanceSource(),
      "vector_namespace_canonical_" + emitMode,
      emitMode,
      "unknown call target: /std/collections/vector/vector");
}

inline void expectCanonicalVectorNamespaceExplicitVectorBindingConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceExplicitVectorBindingSource(),
      "vector_namespace_canonical_explicit_vector_binding_" + emitMode,
      emitMode,
      109);
}

inline void expectStdlibWrapperVectorHelperExplicitVectorBindingConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeStdlibWrapperVectorHelperExplicitVectorBindingSource(),
      "vector_wrapper_explicit_vector_binding_" + emitMode,
      emitMode,
      35);
}

inline void expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeStdlibWrapperVectorHelperExplicitVectorBindingMismatchSource(),
      "vector_wrapper_explicit_vector_binding_mismatch_" + emitMode,
      emitMode,
      "mismatch");
}

inline void expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeStdlibWrapperVectorConstructorExplicitVectorBindingSource(),
      "vector_wrapper_constructor_explicit_vector_binding_" + emitMode,
      emitMode,
      31);
}

inline void expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchContract(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeStdlibWrapperVectorConstructorExplicitVectorBindingMismatchSource(),
      "vector_wrapper_constructor_explicit_vector_binding_mismatch_" + emitMode,
      emitMode,
      2);
}

inline void expectStdlibWrapperVectorConstructorAutoInferenceConformance(const std::string &emitMode) {
  if (emitMode == "vm" || emitMode == "native" || emitMode == "exe") {
    expectVectorConformanceProgramRuns(makePortableStdlibVectorConstructorAutoInferenceSource(),
                                       "vector_constructor_auto_inference_" + emitMode,
                                       emitMode,
                                       52);
    return;
  }
  expectVectorConformanceProgramRuns(
      makeStdlibWrapperVectorConstructorAutoInferenceSource(),
      "vector_wrapper_constructor_auto_inference_" + emitMode,
      emitMode,
      57);
}

inline void expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeStdlibWrapperVectorConstructorAutoInferenceMismatchSource(),
      "vector_wrapper_constructor_auto_inference_mismatch_" + emitMode,
      emitMode,
      "/std/collections/vectorPair",
      "implicit template arguments conflict");
}

inline void expectStdlibWrapperVectorConstructorReceiverConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceCompileReject(
        makeStdlibWrapperVectorConstructorReceiverConformanceSource(),
        "vector_wrapper_constructor_receiver_" + emitMode,
        emitMode,
        "count requires array, vector, map, or string target");
    return;
  }
  if (emitMode == "native" || emitMode == "exe") {
    expectVectorConformanceCompileReject(
        makeStdlibWrapperVectorConstructorReceiverConformanceSource(),
        "vector_wrapper_constructor_receiver_" + emitMode,
        emitMode,
        "count requires array, vector, map, or string target");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeStdlibWrapperVectorConstructorReceiverConformanceSource(),
      "vector_wrapper_constructor_receiver_" + emitMode,
      emitMode,
      136);
}

inline void expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeStdlibWrapperVectorConstructorHelperReceiverMismatchSource(),
      "vector_wrapper_constructor_helper_receiver_mismatch_" + emitMode,
      emitMode,
      "/std/collections/vectorPair",
      "implicit template arguments conflict");
}

inline void expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeStdlibWrapperVectorConstructorMethodReceiverMismatchSource(),
      "vector_wrapper_constructor_method_receiver_mismatch_" + emitMode,
      emitMode,
      "/std/collections/vectorPair",
      "implicit template arguments conflict");
}

inline void expectCanonicalVectorNamespaceNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceNamedArgsSource(),
      "vector_namespace_canonical_named_args_" + emitMode,
      emitMode,
      11);
}

inline void expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorNamespaceNamedArgsTemporaryReceiverSource(),
        "vector_namespace_canonical_named_args_temporary_receiver_" + emitMode,
        emitMode,
        "count requires array, vector, map, or string target");
    return;
  }
  if (emitMode == "native" || emitMode == "exe") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorNamespaceNamedArgsTemporaryReceiverSource(),
        "vector_namespace_canonical_named_args_temporary_receiver_" + emitMode,
        emitMode,
        "count requires array, vector, map, or string target");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceNamedArgsTemporaryReceiverSource(),
      "vector_namespace_canonical_named_args_temporary_receiver_" + emitMode,
      emitMode,
      16);
}

inline void expectCanonicalVectorNamespaceTypeMismatchReject(const std::string &emitMode) {
  const std::string source = makeCanonicalVectorNamespaceTypeMismatchRejectSource();
  const std::string srcPath = writeTemp("vector_namespace_canonical_type_mismatch_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_type_mismatch_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("mismatch") != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_type_mismatch_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}

inline void expectCanonicalVectorNamespaceVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorNamespaceImportRequirementSource(),
                                       "vector_namespace_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/vector");
}

inline void expectCanonicalVectorNamespaceExplicitBindingReject(const std::string &emitMode) {
  const std::string source = makeCanonicalVectorNamespaceExplicitBindingRejectSource();
  const std::string srcPath = writeTemp("vector_namespace_canonical_binding_reject_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_binding_reject_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("mismatch") != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_binding_reject_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}

inline void expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject(const std::string &emitMode) {
  const std::string source = makeCanonicalVectorNamespaceNamedArgsExplicitBindingRejectSource();
  const std::string srcPath =
      writeTemp("vector_namespace_canonical_named_args_binding_reject_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_named_args_binding_reject_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("mismatch") != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_namespace_canonical_named_args_binding_reject_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}

inline void expectCanonicalVectorCountNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorCountNamedArgsSource(),
      "vector_count_canonical_named_args_" + emitMode,
      emitMode,
      2);
}

inline void expectCanonicalVectorCountVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorCountImportRequirementSource(),
                                       "vector_count_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/count");
}

inline void expectCanonicalVectorCapacityNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorCapacityNamedArgsSource(),
      "vector_capacity_canonical_named_args_" + emitMode,
      emitMode,
      2);
}

inline void expectCanonicalVectorCapacityVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorCapacityImportRequirementSource(),
                                       "vector_capacity_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/capacity");
}

inline void expectCanonicalVectorAccessNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorAccessNamedArgsSource(),
      "vector_access_canonical_named_args_" + emitMode,
      emitMode,
      9);
}

inline void expectCanonicalVectorAccessVmImportRequirement(const std::string &helperName) {
  expectVectorConformanceCompileReject(makeCanonicalVectorAccessImportRequirementSource(helperName),
                                       "vector_" + helperName + "_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/" + helperName);
}

inline void expectCanonicalVectorPushNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorPushNamedArgsSource(),
      "vector_push_canonical_named_args_" + emitMode,
      emitMode,
      2);
}

inline void expectCanonicalVectorPushVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorPushImportRequirementSource(),
                                       "vector_push_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/push");
}

inline void expectCanonicalVectorPopNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorPopNamedArgsSource(),
      "vector_pop_canonical_named_args_" + emitMode,
      emitMode,
      1);
}

inline void expectCanonicalVectorDiscardOwnershipConformance(const std::string &emitMode) {
  if (emitMode == "exe" || emitMode == "vm" || emitMode == "native") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorDiscardOwnershipConformanceSource(),
        "vector_discard_canonical_ownership_" + emitMode,
        emitMode,
        "/std/collections/experimental_vector/vectorPop");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorDiscardOwnershipConformanceSource(),
      "vector_discard_canonical_ownership_" + emitMode,
      emitMode,
      0);
}

inline void expectCanonicalVectorIndexedRemovalOwnershipConformance(const std::string &emitMode) {
  if (emitMode == "exe" || emitMode == "vm") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorIndexedRemovalOwnershipConformanceSource(),
        "vector_indexed_removal_canonical_ownership_" + emitMode,
        emitMode,
        "vm backend only supports numeric/bool/string vector literals");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorIndexedRemovalOwnershipConformanceSource(),
      "vector_indexed_removal_canonical_ownership_" + emitMode,
      emitMode,
      8);
}

inline void expectCanonicalVectorPopVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorPopImportRequirementSource(),
                                       "vector_pop_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/pop");
}

inline void expectCanonicalVectorReserveNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorReserveNamedArgsSource(),
      "vector_reserve_canonical_named_args_" + emitMode,
      emitMode,
      10);
}

inline void expectCanonicalVectorReserveVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorReserveImportRequirementSource(),
                                       "vector_reserve_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/reserve");
}

inline void expectCanonicalVectorMutatorBoolReject(const std::string &emitMode,
                                                   const std::string &nameStem,
                                                   const std::string &statementText,
                                                   const std::string &expectedFragment) {
  expectVectorConformanceCompileReject(makeCanonicalVectorMutatorBoolRejectSource(statementText),
                                       nameStem + "_" + emitMode,
                                       emitMode,
                                       expectedFragment);
}

inline void expectCanonicalVectorMutatorNumericRejects(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorMutatorBoolRejectSource("/std/collections/vector/reserve(values, true)"),
      "vector_reserve_bool_call_reject_" + emitMode,
      emitMode,
      3);
  expectCanonicalVectorMutatorBoolReject(emitMode,
                                         "vector_reserve_bool_method_reject",
                                         "values.reserve(true)",
                                         "reserve requires integer capacity");
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorMutatorBoolRejectSource("/std/collections/vector/remove_at(values, true)"),
      "vector_remove_at_bool_call_reject_" + emitMode,
      emitMode,
      2);
  expectCanonicalVectorMutatorBoolReject(emitMode,
                                         "vector_remove_at_bool_method_reject",
                                         "values.remove_at(true)",
                                         "remove_at requires integer index");
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorMutatorBoolRejectSource("/std/collections/vector/remove_swap(values, true)"),
      "vector_remove_swap_bool_call_reject_" + emitMode,
      emitMode,
      2);
  expectCanonicalVectorMutatorBoolReject(emitMode,
                                         "vector_remove_swap_bool_method_reject",
                                         "values.remove_swap(true)",
                                         "remove_swap requires integer index");
}

inline void expectCanonicalVectorReserveReceiverRejects(const std::string &emitMode) {
  expectVectorConformanceCompileReject(makeCanonicalVectorReserveReceiverRejectSource("reserve(values, 8i32)"),
                                       "vector_reserve_array_call_receiver_reject_" + emitMode,
                                       emitMode,
                                       "reserve requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorReserveReceiverRejectSource("values.reserve(8i32)"),
      "vector_reserve_array_method_receiver_reject_" + emitMode,
      emitMode,
      "reserve requires vector binding");
}

inline void expectCanonicalVectorPushReceiverRejects(const std::string &emitMode) {
  expectVectorConformanceCompileReject(makeCanonicalVectorPushReceiverRejectSource("push(values, 8i32)"),
                                       "vector_push_array_call_receiver_reject_" + emitMode,
                                       emitMode,
                                       "push requires vector binding");
  expectVectorConformanceCompileReject(makeCanonicalVectorPushReceiverRejectSource("values.push(8i32)"),
                                       "vector_push_array_method_receiver_reject_" + emitMode,
                                       emitMode,
                                       "push requires vector binding");
}

inline void expectCanonicalVectorMutatorNamedArgExpressionRejects(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgExpressionRejectSource("push([value] 8i32, [values] values)"),
      "vector_push_named_arg_expression_reject_" + emitMode,
      emitMode,
      "push is only supported as a statement");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgExpressionRejectSource("reserve([capacity] 8i32, [values] values)"),
      "vector_reserve_named_arg_expression_reject_" + emitMode,
      emitMode,
      "reserve is only supported as a statement");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgExpressionRejectSource("remove_at([index] 0i32, [values] values)"),
      "vector_remove_at_named_arg_expression_reject_" + emitMode,
      emitMode,
      "remove_at is only supported as a statement");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgExpressionRejectSource("remove_swap([index] 0i32, [values] values)"),
      "vector_remove_swap_named_arg_expression_reject_" + emitMode,
      emitMode,
      "remove_swap is only supported as a statement");
}

inline void expectCanonicalVectorMutatorNamedArgReceiverRejects(const std::string &emitMode) {
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("push([value] 8i32, [values] values)"),
      "vector_push_named_arg_array_call_reject_" + emitMode,
      emitMode,
      "push requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("reserve([capacity] 8i32, [values] values)"),
      "vector_reserve_named_arg_array_call_reject_" + emitMode,
      emitMode,
      "reserve requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("remove_at([index] 0i32, [values] values)"),
      "vector_remove_at_named_arg_array_call_reject_" + emitMode,
      emitMode,
      "remove_at requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("remove_swap([index] 0i32, [values] values)"),
      "vector_remove_swap_named_arg_array_call_reject_" + emitMode,
      emitMode,
      "remove_swap requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("values.push([value] 8i32)"),
      "vector_push_named_arg_array_method_reject_" + emitMode,
      emitMode,
      "push requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("values.reserve([capacity] 8i32)"),
      "vector_reserve_named_arg_array_method_reject_" + emitMode,
      emitMode,
      "reserve requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("values.remove_at([index] 0i32)"),
      "vector_remove_at_named_arg_array_method_reject_" + emitMode,
      emitMode,
      "remove_at requires vector binding");
  expectVectorConformanceCompileReject(
      makeCanonicalVectorMutatorNamedArgReceiverRejectSource("values.remove_swap([index] 0i32)"),
      "vector_remove_swap_named_arg_array_method_reject_" + emitMode,
      emitMode,
      "remove_swap requires vector binding");
}

inline void expectCanonicalVectorMutatorImportRequirement(const std::string &emitMode,
                                                          const std::string &helperName,
                                                          const std::string &callArgs) {
  const std::string source = makeCanonicalVectorMutatorImportRequirementSource(helperName, callArgs);
  const std::string srcPath =
      writeTemp("vector_" + helperName + "_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_" + helperName + "_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();
  const std::string expected = "unknown call target: /std/collections/vector/" + helperName;

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_" + helperName + "_canonical_import_requirement_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find(expected) != std::string::npos);
}

inline void expectBareVectorMutatorImportRequirement(const std::string &emitMode,
                                                     const std::string &helperName,
                                                     const std::string &callArgs) {
  const std::string source = makeBareVectorMutatorImportRequirementSource(helperName, callArgs);
  const std::string srcPath =
      writeTemp("vector_bare_" + helperName + "_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_bare_" + helperName + "_import_requirement_" + emitMode + "_out.txt"))
          .string();
  const std::string expected = "unknown call target: /std/collections/vector/" + helperName;

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  if (emitMode == "native" || emitMode == "exe") {
    const std::string discardExePath =
        (testScratchPath("") /
         ("primec_vector_bare_" + helperName + "_import_requirement_" + emitMode + "_discard_exe"))
            .string();
    const std::string compileCmd =
        "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
        quoteShellArg(discardExePath) + " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(compileCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_bare_" + helperName + "_import_requirement_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find(expected) != std::string::npos);
}

inline void expectBareVectorMutatorMethodImportRequirement(const std::string &emitMode,
                                                           const std::string &helperName,
                                                           const std::string &methodArgs) {
  const std::string source = makeBareVectorMutatorMethodImportRequirementSource(helperName, methodArgs);
  const std::string srcPath =
      writeTemp("vector_bare_" + helperName + "_method_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_vector_bare_" + helperName + "_method_import_requirement_" + emitMode + "_out.txt"))
          .string();
  const std::string expected = "unknown method: /std/collections/vector/" + helperName;

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  if (emitMode == "native" || emitMode == "exe") {
    const std::string compileCmd =
        "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o /dev/null --entry /main > " +
        quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(compileCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  const std::string discardExePath =
      (testScratchPath("") /
       ("primec_vector_bare_" + helperName + "_method_import_requirement_" + emitMode + "_discard_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(discardExePath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find(expected) != std::string::npos);
}

inline void expectCanonicalVectorClearImportRequirement(const std::string &emitMode) {
  expectCanonicalVectorMutatorImportRequirement(emitMode, "clear", "values");
}

inline void expectCanonicalVectorRemoveAtImportRequirement(const std::string &emitMode) {
  expectCanonicalVectorMutatorImportRequirement(emitMode, "remove_at", "values, 1i32");
}

inline void expectCanonicalVectorRemoveSwapImportRequirement(const std::string &emitMode) {
  expectCanonicalVectorMutatorImportRequirement(emitMode, "remove_swap", "values, 1i32");
}

inline void expectCanonicalVectorNamespaceCountShadow(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceCountShadowSource(),
      "vector_namespace_canonical_count_shadow_" + emitMode,
      emitMode,
      91);
}

inline void expectCanonicalVectorNamespacePushShadow(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespacePushShadowSource(),
      "vector_namespace_canonical_push_shadow_" + emitMode,
      emitMode,
      99);
}

inline void expectCanonicalVectorNamespaceTemporaryReceiverConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorNamespaceTemporaryReceiverSource(),
        "vector_namespace_canonical_temporary_receiver_" + emitMode,
        emitMode,
        "count requires array, vector, map, or string target");
    return;
  }
  if (emitMode == "native" || emitMode == "exe") {
    expectVectorConformanceCompileReject(
        makeCanonicalVectorNamespaceTemporaryReceiverSource(),
        "vector_namespace_canonical_temporary_receiver_" + emitMode,
        emitMode,
        "unknown call target: /std/collections/vector/vector");
    return;
  }
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceTemporaryReceiverSource(),
      "vector_namespace_canonical_temporary_receiver_" + emitMode,
      emitMode,
      15);
}

inline void expectVectorHelperRuntimeContract(const std::string &emitMode,
                                              const std::string &importPath,
                                              const std::string &mode) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorHelperRuntimeContractSource(importPath, mode);
  const std::string srcPath = writeTemp("vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (testScratchPath("") /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_err.txt"))
          .string();
  const bool experimentalPath = importPath == "/std/collections/experimental_vector/*";
  std::string expectedError;
  if (emitMode != "vm" && experimentalPath) {
    expectedError = "array index out of bounds\n";
  } else {
    expectedError = mode == "pop_empty"         ? "container empty\n"
                    : mode == "reserve_negative" ? "vector reserve expects non-negative capacity\n"
                    : mode == "reserve_growth_overflow"
                        ? "array index out of bounds\n"
                    : mode == "push_growth_overflow"
                        ? (experimentalPath ? "array index out of bounds\n"
                                            : "vector reserve expects non-negative capacity\n")
                                             : "array index out of bounds\n";
  }

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (testScratchPath("") /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == expectedError);
}
