#pragma once

inline void expectNativeMapConformanceProgramRunsOrCompileRejectWithOutput(
    const std::string &source,
    const std::string &nameStem,
    int expectedExitCode,
    const std::string &expectedOutput,
    const std::string &expectedCompileRejectFragment) {
  const std::string emitMode = "native";
  const std::string srcPath = writeTemp(nameStem + "_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  const std::string exePath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main > " + quoteShellArg(outPath) +
                                 " 2>&1";
  const int compileStatus = runCommand(compileCmd);
  if (compileStatus == 0) {
    const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == expectedOutput);
    return;
  }

  CHECK(compileStatus != 0);
  const std::string output = readFile(outPath);
  if (output.empty()) {
    CHECK(compileStatus == -1);
  } else {
    INFO(output);
    CHECK(output.find(expectedCompileRejectFragment) != std::string::npos);
  }
}

inline void expectCanonicalMapNamespaceExperimentalValueConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalValueConformanceSource(),
                                    "map_namespace_canonical_experimental_value",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectCanonicalMapNamespaceExperimentalConstructorConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalConstructorConformanceSource(),
                                    "map_namespace_canonical_experimental_constructor",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectExperimentalMapOwnershipMethodConformance(const std::string &emitMode) {
  // TODO-4741: experimental Map<K,V> constructors (mapSingle) are
  // unimplemented; vm now rejects at compile time like exe/native.
  const std::string source = makeExperimentalMapOwnershipMethodConformanceSource();
  const std::string srcPath = writeTemp("map_experimental_ownership_method_" + emitMode + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / ("map_experimental_ownership_method_" + emitMode + "_out.txt")).string();
  const std::string artifactPath =
      (testScratchPath("") / ("map_experimental_ownership_method_" + emitMode + "_artifact")).string();

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(artifactPath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
}

inline void expectCanonicalMapNamespaceExperimentalReturnConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalReturnConformanceSource(),
                                    "map_namespace_canonical_experimental_return",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectCanonicalMapNamespaceExperimentalParameterConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectNativeMapConformanceProgramRunsOrCompileRejectWithOutput(
        makeCanonicalMapNamespaceExperimentalParameterConformanceSource(),
        "map_namespace_canonical_experimental_parameter",
        18,
        "2\n4\n4\n7\n1\n",
        "argument type mismatch for /scoreValues parameter values");
    return;
  }

  expectMapConformanceCompileReject(makeCanonicalMapNamespaceExperimentalParameterConformanceSource(),
                                    "map_namespace_canonical_experimental_parameter",
                                    emitMode,
                                    "argument type mismatch for /scoreValues parameter values");
}

inline void expectWrapperMapConstructorExperimentalBindingConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrapperMapConstructorExperimentalBindingConformanceSource(),
                                    "map_wrapper_constructor_experimental_binding",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectWrapperMapConstructorExperimentalReturnConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrapperMapConstructorExperimentalReturnConformanceSource(),
                                    "map_wrapper_constructor_experimental_return",
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrapperMapConstructorExperimentalParameterConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrapperMapConstructorExperimentalParameterConformanceSource(),
                                    "map_wrapper_constructor_experimental_parameter",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectWrappedExperimentalMapParameterConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapParameterConformanceSource(),
                                    "map_wrapped_experimental_parameter",
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrappedExperimentalMapBindingConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapBindingConformanceSource(),
                                    "map_wrapped_experimental_binding_" + emitMode,
                                    emitMode,
                                    "unable to infer implicit template arguments for /wrapValues");
}

inline void expectWrappedExperimentalMapAssignConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapAssignConformanceSource(),
                                    "map_wrapped_experimental_assign_" + emitMode,
                                    emitMode,
                                    "unable to infer implicit template arguments for /wrapValues");
}

inline void expectWrappedExperimentalMapResultFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapResultFieldAssignConformanceSource(),
                                    "map_wrapped_experimental_result_field_assign_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrappedExperimentalMapResultDerefFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapResultDerefFieldAssignConformanceSource(),
                                    "map_wrapped_experimental_result_deref_field_assign_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrappedExperimentalMapStorageFieldConformance(const std::string &emitMode) {
  if (emitMode == "exe") {
    expectMapConformanceCompileReject(
        makeWrappedExperimentalMapStorageFieldConformanceSource(),
        "map_wrapped_experimental_storage_field_" + emitMode,
        emitMode,
        "template arguments are only supported on templated definitions: /Map");
    return;
  }

  if (emitMode == "native") {
    expectNativeMapConformanceProgramRunsOrCompileRejectWithOutput(
        makeWrappedExperimentalMapStorageFieldConformanceSource(),
        "map_wrapped_experimental_storage_field_" + emitMode,
        9,
        "9\n",
        "template arguments are only supported on templated definitions: /Map");
    return;
  }

  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(
      makeWrappedExperimentalMapStorageFieldConformanceSource(),
      "map_wrapped_experimental_storage_field_" + emitMode,
      emitMode,
      "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrappedExperimentalMapStorageDerefFieldConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeWrappedExperimentalMapStorageDerefFieldConformanceSource(),
                                    "map_wrapped_experimental_storage_deref_field_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectWrapperMapHelperExperimentalValueConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(makeWrapperMapHelperExperimentalValueConformanceSource(),
                                    "map_wrapper_helper_experimental_value",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectExperimentalMapAssignConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapAssignConformanceSource(),
                                    "map_experimental_assign",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectImplicitMapAutoInferenceConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeImplicitMapAutoInferenceConformanceSource(),
      "map_implicit_auto_inference_" + emitMode,
      emitMode,
      "unknown call target: /std/collections/mapPair");
}

inline void expectInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  const std::string backendLabel = emitMode == "vm" ? "vm" : "native";
  // TODO-4741: experimental Map<K,V> is unimplemented; vm now rejects the
  // same as exe/native instead of running.
  expectMapConformanceCompileReject(makeInferredExperimentalMapReturnConformanceSource(),
                                    "map_inferred_experimental_return",
                                    emitMode,
                                    backendLabel + " backend only supports indexing into string literals or string bindings");
}

inline void expectBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeBlockInferredExperimentalMapReturnConformanceSource(),
                                    "map_block_inferred_experimental_return",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectAutoBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeAutoBlockInferredExperimentalMapReturnConformanceSource(),
                                    "map_auto_block_inferred_experimental_return",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectWrappedInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeWrappedInferredExperimentalMapReturnConformanceSource(),
      "map_wrapped_inferred_experimental_return_" + emitMode,
      emitMode,
      "unable to infer implicit template arguments for /wrapValues");
}

inline void expectInferredExperimentalMapCallReceiverConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(
        makeInferredExperimentalMapCallReceiverConformanceSource(),
        "map_inferred_experimental_call_receiver_" + emitMode,
        emitMode,
        "");
    return;
  }

  expectMapConformanceCompileReject(
      makeInferredExperimentalMapCallReceiverConformanceSource(),
      "map_inferred_experimental_call_receiver_" + emitMode,
      emitMode,
      "unknown call target: /std/collections/mapPair");
}

inline void expectExperimentalMapStructFieldConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapStructFieldConformanceSource(),
                                    "map_experimental_struct_fields",
                                    emitMode,
                                    emitMode == "native" ? "error:" : "unknown call target: /std/collections/mapPair");
}

inline void expectInferredExperimentalMapStructFieldConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceCompileReject(
        makeInferredExperimentalMapStructFieldConformanceSource(),
        "map_experimental_inferred_struct_fields_" + emitMode,
        emitMode,
        "unknown call target: /std/collections/mapPair");
    return;
  }
  if (emitMode == "native") {
    expectMapConformanceCompileReject(
        makeInferredExperimentalMapStructFieldConformanceSource(),
        "map_experimental_inferred_struct_fields_" + emitMode,
        emitMode,
        "");
    return;
  }

  expectMapConformanceCompileReject(makeInferredExperimentalMapStructFieldConformanceSource(),
                                    "map_experimental_inferred_struct_fields_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectWrappedInferredExperimentalMapStructFieldConformance(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectMapConformanceCompileReject(
        makeWrappedInferredExperimentalMapStructFieldConformanceSource(),
        "map_wrapped_inferred_experimental_struct_fields_" + emitMode,
        emitMode,
        "unknown call target: /std/collections/mapPair");
    return;
  }
  if (emitMode == "native") {
    expectMapConformanceCompileReject(
        makeWrappedInferredExperimentalMapStructFieldConformanceSource(),
        "map_wrapped_inferred_experimental_struct_fields_" + emitMode,
        emitMode,
        "");
    return;
  }

  expectMapConformanceCompileReject(makeWrappedInferredExperimentalMapStructFieldConformanceSource(),
                                    "map_wrapped_inferred_experimental_struct_fields_" + emitMode,
                                    emitMode,
                                    "native backend");
}

inline void expectExperimentalMapMethodParameterConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeExperimentalMapMethodParameterConformanceSource(),
      "map_experimental_method_parameter_" + emitMode,
      emitMode,
      emitMode == "native" ? "" : "argument type mismatch for /Holder/score parameter values");
}

inline void expectInferredExperimentalMapParameterConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeInferredExperimentalMapParameterConformanceSource(),
      "map_experimental_inferred_parameter_" + emitMode,
      emitMode,
      emitMode == "native" ? "" : "unable to infer implicit template arguments for /Holder/score");
}

inline void expectInferredExperimentalMapDefaultParameterConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeInferredExperimentalMapDefaultParameterConformanceSource(),
      "map_experimental_inferred_default_parameter_" + emitMode,
      emitMode,
      emitMode == "native" ? "" : "unable to infer implicit template arguments for /scoreValues");
}

inline void expectWrappedInferredExperimentalMapDefaultParameterConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(
        makeWrappedInferredExperimentalMapDefaultParameterConformanceSource(),
        "map_wrapped_inferred_experimental_default_parameter_" + emitMode,
        emitMode,
        "");
    return;
  }

  if (emitMode == "vm") {
    expectMapConformanceCompileReject(
        makeWrappedInferredExperimentalMapDefaultParameterConformanceSource(),
        "map_wrapped_inferred_experimental_default_parameter_" + emitMode,
        emitMode,
        "unable to infer implicit template arguments for /Holder/score");
    return;
  }

  expectMapConformanceCompileReject(makeWrappedInferredExperimentalMapDefaultParameterConformanceSource(),
                                    "map_wrapped_inferred_experimental_default_parameter_" + emitMode,
                                    emitMode,
                                    "template arguments required for /std/collections/experimental_map/mapNew");
}

inline void expectExperimentalMapHelperReceiverConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeExperimentalMapHelperReceiverConformanceSource(),
      "map_experimental_helper_receiver_" + emitMode,
      emitMode,
      "unknown call target: /std/collections/mapPair");
}

inline void expectWrappedExperimentalMapHelperReceiverConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeWrappedExperimentalMapHelperReceiverConformanceSource(),
      "map_wrapped_experimental_helper_receiver_" + emitMode,
      emitMode,
      "unable to infer implicit template arguments for /wrapValues");
}

inline void expectExperimentalMapMethodReceiverConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(makeExperimentalMapMethodReceiverConformanceSource(),
                                      "map_experimental_method_receiver",
                                      emitMode,
                                      "");
    return;
  }

  expectMapConformanceCompileReject(makeExperimentalMapMethodReceiverConformanceSource(),
                                    "map_experimental_method_receiver",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectWrappedExperimentalMapMethodReceiverConformance(const std::string &emitMode) {
  if (emitMode == "native") {
    expectMapConformanceCompileReject(makeWrappedExperimentalMapMethodReceiverConformanceSource(),
                                      "map_wrapped_experimental_method_receiver_" + emitMode,
                                      emitMode,
                                      "");
    return;
  }

  expectMapConformanceCompileReject(makeWrappedExperimentalMapMethodReceiverConformanceSource(),
                                    "map_wrapped_experimental_method_receiver_" + emitMode,
                                    emitMode,
                                    "unable to infer implicit template arguments for /wrapValues");
}

inline void expectExperimentalMapFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapFieldAssignConformanceSource(),
                                    "map_experimental_field_assign",
                                    emitMode,
                                    "unknown call target: /std/collections/mapPair");
}

inline void expectExperimentalMapStorageReferenceConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(makeExperimentalMapStorageReferenceConformanceSource(),
                                    "map_experimental_storage_reference_" + emitMode,
                                    emitMode,
                                    "template arguments are only supported on templated definitions: /Map");
}

inline void expectCanonicalMapNamespaceExperimentalBorrowedRefConformance(const std::string &emitMode) {
  expectMapConformanceCompileReject(
      makeCanonicalMapNamespaceExperimentalBorrowedRefConformanceSource(),
      "map_namespace_canonical_experimental_borrowed_ref_" + emitMode,
      emitMode,
      emitMode == "native" ? "" : "template arguments are only supported on templated definitions: /Map");
}

inline void expectCanonicalMapNamespaceNamedArgsVmConformance() {
  expectMapVmProgramRunsWithOutput(makeCanonicalMapNamespaceNamedArgsSource(),
                                   "map_namespace_canonical_named_args_vm",
                                   17,
                                   "2\n7\n4\n4\n");
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
  source += "import /std/collections/map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedMapTryAtStringError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<i32, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapTryAtStringError>]\n";
  source += "main() {\n";
  source += "  [Map<i32, string>] values{mapPair<i32, string>(11i32, \"alpha\"utf8, 22i32, \"beta\"utf8)}\n";
  source += "  [string] found{try(/std/collections/map/tryAt<i32, string>(values, 11i32))}\n";
  source += "  [Result<string, ContainerError>] missing{/std/collections/map/tryAt<i32, string>(values, 99i32)}\n";
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
  source += "import /std/collections/map/*\n\n";
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [Map<i32, i32>] values{mapPair<i32, i32>(11i32, 7i32, 22i32, 9i32)}\n";
  source += "  return(/std/collections/map/at<i32, i32>(values, 99i32))\n";
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
  source += "import /std/collections/map/*\n\n";
  source += "[return<int>]\n";
  source += "main([array<string>] args) {\n";
  source += "  [string] key{args[0i32]}\n";
  if (mode == "lookup_argv") {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(\"a\"raw_utf8, 1i32)}\n";
    source += "  return(/std/collections/map/at_unsafe<string, i32>(values, key))\n";
  } else {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(key, 1i32)}\n";
    source += "  return(/std/collections/map/count<string, i32>(values))\n";
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
