#pragma once

inline void expectCanonicalMapNamespaceExperimentalValueConformance(const std::string &emitMode) {
  const std::string expectedOutput =
      emitMode == "exe" ? "4\ncontainer missing key\n2\n4\n7\n1\n2\n" : "4\n\n2\n4\n7\n1\n2\n";
  expectMapConformanceProgramRunsWithOutput(makeCanonicalMapNamespaceExperimentalValueConformanceSource(),
                                            "map_namespace_canonical_experimental_value",
                                            emitMode,
                                            20,
                                            expectedOutput);
}

inline void expectCanonicalMapNamespaceExperimentalConstructorConformance(const std::string &emitMode) {
  const std::string expectedOutput =
      emitMode == "exe" ? "4\ncontainer missing key\n2\n4\n7\n1\n2\n" : "4\n\n2\n4\n7\n1\n2\n";
  expectMapConformanceProgramRunsWithOutput(makeCanonicalMapNamespaceExperimentalConstructorConformanceSource(),
                                            "map_namespace_canonical_experimental_constructor",
                                            emitMode,
                                            20,
                                            expectedOutput);
}

inline void expectExperimentalMapOwnershipMethodConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(makeExperimentalMapOwnershipMethodConformanceSource(),
                                              "map_experimental_ownership_method",
                                              emitMode,
                                              33,
                                              "\n");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapOwnershipMethodConformanceSource(),
                                    "map_experimental_ownership_method",
                                    emitMode,
                                    "native backend only supports numeric/bool map values");
}

inline void expectCanonicalMapNamespaceExperimentalReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeCanonicalMapNamespaceExperimentalReturnConformanceSource(),
                                            "map_namespace_canonical_experimental_return",
                                            emitMode,
                                            18,
                                            "2\n4\n4\n7\n1\n");
}

inline void expectCanonicalMapNamespaceExperimentalParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(makeCanonicalMapNamespaceExperimentalParameterConformanceSource(),
                                              "map_namespace_canonical_experimental_parameter",
                                              emitMode,
                                              18,
                                              "2\n4\n4\n7\n1\n");
    return;
  }

  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalParameterConformanceSource(),
                                    "map_namespace_canonical_experimental_parameter",
                                    emitMode,
                                    "native backend requires typed map bindings for at");
}

inline void expectWrapperMapConstructorExperimentalBindingConformance(const std::string &emitMode) {
  const std::string expectedOutput =
      emitMode == "exe" ? "4\ncontainer missing key\n2\n4\n7\n1\n2\n" : "4\n\n2\n4\n7\n1\n2\n";
  expectMapConformanceProgramRunsWithOutput(makeWrapperMapConstructorExperimentalBindingConformanceSource(),
                                            "map_wrapper_constructor_experimental_binding",
                                            emitMode,
                                            20,
                                            expectedOutput);
}

inline void expectWrapperMapConstructorExperimentalReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrapperMapConstructorExperimentalReturnConformanceSource(),
                                            "map_wrapper_constructor_experimental_return",
                                            emitMode,
                                            18,
                                            "2\n4\n4\n7\n1\n");
}

inline void expectWrapperMapConstructorExperimentalParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(makeWrapperMapConstructorExperimentalParameterConformanceSource(),
                                              "map_wrapper_constructor_experimental_parameter",
                                              emitMode,
                                              18,
                                              "2\n4\n4\n7\n1\n");
    return;
  }

  expectMapConformanceCompileReject(makeWrapperMapConstructorExperimentalParameterConformanceSource(),
                                    "map_wrapper_constructor_experimental_parameter",
                                    emitMode,
                                    "native backend requires typed map bindings for at");
}

inline void expectWrappedExperimentalMapParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapParameterConformanceSource(),
                                              "map_wrapped_experimental_parameter",
                                              emitMode,
                                              19,
                                              "3\n4\n3\n9\n");
    return;
  }

  expectMapConformanceCompileReject(
      makeWrappedExperimentalMapParameterConformanceSource(),
      "map_wrapped_experimental_parameter",
      emitMode,
      "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions");
}

inline void expectWrappedExperimentalMapBindingConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapBindingConformanceSource(),
                                            "map_wrapped_experimental_binding_" + emitMode,
                                            emitMode,
                                            13,
                                            "4\n9\n");
}

inline void expectWrappedExperimentalMapAssignConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapAssignConformanceSource(),
                                            "map_wrapped_experimental_assign_" + emitMode,
                                            emitMode,
                                            13,
                                            "4\n9\n");
}

inline void expectWrappedExperimentalMapResultFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapResultFieldAssignConformanceSource(),
                                            "map_wrapped_experimental_result_field_assign_" + emitMode,
                                            emitMode,
                                            6,
                                            "6\n");
}

inline void expectWrappedExperimentalMapResultDerefFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapResultDerefFieldAssignConformanceSource(),
                                            "map_wrapped_experimental_result_deref_field_assign_" + emitMode,
                                            emitMode,
                                            11,
                                            "2\n9\n");
}

inline void expectWrappedExperimentalMapStorageFieldConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapStorageFieldConformanceSource(),
                                            "map_wrapped_experimental_storage_field_" + emitMode,
                                            emitMode,
                                            9,
                                            "9\n");
}

inline void expectWrappedExperimentalMapStorageDerefFieldConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrappedExperimentalMapStorageDerefFieldConformanceSource(),
                                            "map_wrapped_experimental_storage_deref_field_" + emitMode,
                                            emitMode,
                                            9,
                                            "9\n");
}

inline void expectWrapperMapHelperExperimentalValueConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeWrapperMapHelperExperimentalValueConformanceSource(),
                                            "map_wrapper_helper_experimental_value",
                                            emitMode,
                                            21,
                                            "2\n4\n9\n5\n1\n");
}

inline void expectExperimentalMapAssignConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(makeExperimentalMapAssignConformanceSource(),
                                              "map_experimental_assign",
                                              emitMode,
                                              36,
                                              "2\n4\n4\n7\n1\n2\n4\n4\n7\n1\n");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapAssignConformanceSource(),
                                    "map_experimental_assign",
                                    emitMode,
                                    "native backend requires typed map bindings for at");
}

inline void expectImplicitMapAutoInferenceConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeImplicitMapAutoInferenceConformanceSource(),
      "map_implicit_auto_inference_" + emitMode,
      emitMode,
      19);
}

inline void expectInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeInferredExperimentalMapReturnConformanceSource(),
                                            "map_inferred_experimental_return",
                                            emitMode,
                                            16,
                                            "3\n4\n9\n");
}

inline void expectBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeBlockInferredExperimentalMapReturnConformanceSource(),
                                            "map_block_inferred_experimental_return",
                                            emitMode,
                                            16,
                                            "3\n4\n9\n");
}

inline void expectAutoBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeAutoBlockInferredExperimentalMapReturnConformanceSource(),
                                            "map_auto_block_inferred_experimental_return",
                                            emitMode,
                                            16,
                                            "3\n4\n9\n");
}

inline void expectWrappedInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(
      makeWrappedInferredExperimentalMapReturnConformanceSource(),
      "map_wrapped_inferred_experimental_return_" + emitMode,
      emitMode,
      11,
      "2\n4\n5\n");
}

inline void expectInferredExperimentalMapCallReceiverConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeInferredExperimentalMapCallReceiverConformanceSource(),
      "map_inferred_experimental_call_receiver_" + emitMode,
      emitMode,
      "try requires Result argument");
}

inline void expectExperimentalMapStructFieldConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapStructFieldConformanceSource(),
                                    "map_experimental_struct_fields",
                                    emitMode,
                                    "block expression requires a value");
}

inline void expectInferredExperimentalMapStructFieldConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRuns(
        makeInferredExperimentalMapStructFieldConformanceSource(),
        "map_experimental_inferred_struct_fields_" + emitMode,
        emitMode,
        13);
    return;
  }

  expectMapConformanceCompileReject(makeInferredExperimentalMapStructFieldConformanceSource(),
                                    "map_experimental_inferred_struct_fields_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectWrappedInferredExperimentalMapStructFieldConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRuns(
        makeWrappedInferredExperimentalMapStructFieldConformanceSource(),
        "map_wrapped_inferred_experimental_struct_fields_" + emitMode,
        emitMode,
        13);
    return;
  }

  expectMapConformanceCompileReject(makeWrappedInferredExperimentalMapStructFieldConformanceSource(),
                                    "map_wrapped_inferred_experimental_struct_fields_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectExperimentalMapMethodParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(
        makeExperimentalMapMethodParameterConformanceSource(),
        "map_experimental_method_parameter_" + emitMode,
        emitMode,
        10,
        "2\n4\n2\n2\n6\n4\n");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapMethodParameterConformanceSource(),
                                    "map_experimental_method_parameter_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectInferredExperimentalMapParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(
        makeInferredExperimentalMapParameterConformanceSource(),
        "map_experimental_inferred_parameter_" + emitMode,
        emitMode,
        19,
        "3\n4\n3\n9\n7\n12\n");
    return;
  }

  expectMapConformanceCompileReject(makeInferredExperimentalMapParameterConformanceSource(),
                                    "map_experimental_inferred_parameter_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectInferredExperimentalMapDefaultParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(
        makeInferredExperimentalMapDefaultParameterConformanceSource(),
        "map_experimental_inferred_default_parameter_" + emitMode,
        emitMode,
        19,
        "2\n4\n1\n4\n3\n5\n6\n5\n8\n");
    return;
  }

  expectMapConformanceCompileReject(makeInferredExperimentalMapDefaultParameterConformanceSource(),
                                    "map_experimental_inferred_default_parameter_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectWrappedInferredExperimentalMapDefaultParameterConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRunsWithOutput(
        makeWrappedInferredExperimentalMapDefaultParameterConformanceSource(),
        "map_wrapped_inferred_experimental_default_parameter_" + emitMode,
        emitMode,
        37,
        "3\n4\n4\n9\n2\n4\n2\n9\n7\n13\n6\n11\n");
    return;
  }

  expectMapConformanceCompileReject(makeWrappedInferredExperimentalMapDefaultParameterConformanceSource(),
                                    "map_wrapped_inferred_experimental_default_parameter_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectExperimentalMapHelperReceiverConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(
      makeExperimentalMapHelperReceiverConformanceSource(),
      "map_experimental_helper_receiver_" + emitMode,
      emitMode,
      21,
      "2\n4\n9\n5\n1\n");
}

inline void expectWrappedExperimentalMapHelperReceiverConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRuns(makeWrappedExperimentalMapHelperReceiverConformanceSource(),
                                    "map_wrapped_experimental_helper_receiver_" + emitMode,
                                    emitMode,
                                    16);
    return;
  }

  expectMapConformanceCompileReject(makeWrappedExperimentalMapHelperReceiverConformanceSource(),
                                    "map_wrapped_experimental_helper_receiver_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectExperimentalMapMethodReceiverConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(makeExperimentalMapMethodReceiverConformanceSource(),
                                  "map_experimental_method_receiver",
                                  emitMode,
                                  15);
}

inline void expectWrappedExperimentalMapMethodReceiverConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(makeWrappedExperimentalMapMethodReceiverConformanceSource(),
                                  "map_wrapped_experimental_method_receiver_" + emitMode,
                                  emitMode,
                                  16);
}

inline void expectExperimentalMapFieldAssignConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceProgramRuns(makeExperimentalMapFieldAssignConformanceSource(),
                                    "map_experimental_field_assign",
                                    emitMode,
                                    13);
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapFieldAssignConformanceSource(),
                                    "map_experimental_field_assign",
                                    emitMode,
                                    "native backend");
}

inline void expectExperimentalMapStorageReferenceConformance(const std::string &emitMode) {
  expectMapConformanceProgramRunsWithOutput(makeExperimentalMapStorageReferenceConformanceSource(),
                                            "map_experimental_storage_reference_" + emitMode,
                                            emitMode,
                                            7,
                                            "2\n5\n");
}

inline void expectCanonicalMapNamespaceExperimentalBorrowedRefConformance(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceExperimentalBorrowedRefConformanceSource();
  const std::string srcPath =
      writeTemp("map_namespace_canonical_experimental_borrowed_ref_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_map_namespace_canonical_experimental_borrowed_ref_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 27);
    CHECK(readFile(outPath) == "\n");
    return;
  }

  const std::string artifactPath =
      (testScratchPath("") /
       ("primec_map_namespace_canonical_experimental_borrowed_ref_" + emitMode + "_artifact"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(artifactPath) + " --entry /main > " + quoteShellArg(outPath) +
                                 " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("native backend") != std::string::npos);
}

inline void expectCanonicalMapNamespaceNamedArgsVmConformance() {
  expectMapVmProgramRunsWithOutput(makeCanonicalMapNamespaceNamedArgsSource(),
                                   "map_namespace_canonical_named_args_vm",
                                   36,
                                   "2\n2\n288\n0\n");
}

inline void expectCanonicalMapNamespaceTypeMismatchReject(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceTypeMismatchRejectSource();
  const std::string srcPath = writeTemp("map_namespace_canonical_type_mismatch_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_map_namespace_canonical_type_mismatch_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("mismatch") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}

inline void expectCanonicalMapNamespaceVmImportRequirement() {
  expectMapConformanceCompileReject(makeCanonicalMapNamespaceImportRequirementSource(),
                                    "map_namespace_canonical_import_requirement",
                                    "vm",
                                    "unknown call target: /std/collections/map/map");
}

inline void expectCanonicalMapCountVmBuiltinRejectsTemplateArgs() {
  expectMapConformanceCompileReject(makeCanonicalMapCountImportRequirementSource(),
                                    "map_count_canonical_vm_builtin_template_reject",
                                    "vm",
                                    "count does not accept template arguments");
}

inline void expectCanonicalMapContainsVmImportRequirement() {
  expectMapConformanceCompileReject(makeCanonicalMapContainsImportRequirementSource(),
                                    "map_contains_canonical_import_requirement",
                                    "vm",
                                    "unknown call target: /std/collections/map/contains");
}

inline void expectCanonicalMapTryAtVmImportRequirement() {
  expectMapConformanceCompileReject(makeCanonicalMapTryAtImportRequirementSource(),
                                    "map_try_at_canonical_import_requirement",
                                    "vm",
                                    "unknown call target: /std/collections/map/tryAt");
}

inline void expectCanonicalMapAccessVmBuiltinConformance(const std::string &helperName) {
  const std::string source = makeCanonicalMapAccessImportRequirementSource(helperName);
  const std::string srcPath =
      writeTemp("map_" + helperName + "_canonical_vm_builtin.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       ("primec_map_" + helperName + "_canonical_vm_builtin_out.txt"))
          .string();
  const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                             quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath).empty());
}

inline void expectCanonicalMapNamespaceCountShadow(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceCountShadowSource(),
      "map_namespace_canonical_count_shadow_" + emitMode,
      emitMode,
      91);
}

inline void expectCanonicalMapNamespaceAccessShadow(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceAccessShadowSource(),
      "map_namespace_canonical_access_shadow_" + emitMode,
      emitMode,
      121);
}

inline std::string makeExperimentalMapTryAtStringConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedMapTryAtStringError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<i32, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapTryAtStringError>]\n";
  source += "main() {\n";
  source += "  [Map<i32, string>] values{mapPair<i32, string>(11i32, \"alpha\"utf8, 22i32, \"beta\"utf8)}\n";
  source += "  [string] found{try(mapTryAt<i32, string>(values, 11i32))}\n";
  source += "  [Result<string, ContainerError>] missing{mapTryAt<i32, string>(values, 99i32)}\n";
  source += "  print_line(found)\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(plus(count(found), 23i32)))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalMapTryAtStringConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapTryAtStringConformanceSource();
  const std::string srcPath = writeTemp("map_try_at_experimental_string_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / ("primec_map_try_at_experimental_string_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 28);
    CHECK(readFile(outPath) == "alpha\n\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_map_try_at_experimental_string_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 28);
  CHECK(readFile(outPath) == "alpha\n\n");
}

inline std::string makeExperimentalMapAtMissingConformanceSource() {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [Map<i32, i32>] values{mapPair<i32, i32>(11i32, 7i32, 22i32, 9i32)}\n";
  source += "  return(mapAt<i32, i32>(values, 99i32))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalMapAtMissingConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapAtMissingConformanceSource();
  const std::string srcPath = writeTemp("map_at_missing_experimental_" + emitMode + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / ("primec_map_at_missing_experimental_" + emitMode + "_err.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == "map key not found\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_map_at_missing_experimental_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

inline std::string makeExperimentalMapStringKeyRejectSource(const std::string &mode) {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<int>]\n";
  source += "main([array<string>] args) {\n";
  source += "  [string] key{args[0i32]}\n";
  if (mode == "lookup_argv") {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(\"a\"raw_utf8, 1i32)}\n";
    source += "  return(mapAtUnsafe<string, i32>(values, key))\n";
  } else {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(key, 1i32)}\n";
    source += "  return(mapCount<string, i32>(values))\n";
  }
  source += "}\n";
  return source;
}

inline void expectExperimentalMapStringKeyReject(const std::string &emitMode,
                                                 const std::string &mode) {
  const std::string source = makeExperimentalMapStringKeyRejectSource(mode);
  const std::string srcPath =
      writeTemp("map_string_key_" + mode + "_experimental_" + emitMode + ".prime", source);
  const std::string errPath =
      (testScratchPath("") /
       ("primec_map_string_key_" + mode + "_experimental_" + emitMode + "_err.txt"))
          .string();
  const std::string expectedError =
      "Semantic error: entry argument strings are only supported in print calls or string bindings";

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(errPath).find(expectedError) != std::string::npos);
    return;
  }

  const std::string compileCmd =
      "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(expectedError) != std::string::npos);
  if (emitMode == "native") {
    CHECK(diagnostics.find(": error: " + expectedError) != std::string::npos);
    CHECK(diagnostics.find("^") != std::string::npos);
  }
}
