#pragma once

inline void expectExperimentalVectorOwnershipConformance(const std::string &emitMode) {
  if (emitMode != "vm") {
    return;
  }

  expectVectorConformanceProgramRuns(makeExperimentalVectorOwnedDropConformanceSource(),
                                     "experimental_vector_owned_drop_" + emitMode,
                                     emitMode,
                                     0);
  expectVectorConformanceProgramRuns(makeExperimentalVectorRelocationConformanceSource(),
                                     "experimental_vector_owned_relocation_" + emitMode,
                                     emitMode,
                                     73);
  expectVectorConformanceProgramRuns(makeExperimentalVectorRemovalConformanceSource(),
                                     "experimental_vector_owned_removal_" + emitMode,
                                     emitMode,
                                     155);
}

inline void expectExperimentalVectorVariadicConstructorConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorVariadicConstructorSource(),
                                     "experimental_vector_variadic_ctor_" + emitMode,
                                     emitMode,
                                     18);
}

inline void expectExperimentalVectorVariadicConstructorMismatchReject(const std::string &emitMode) {
  if (emitMode == "vm") {
    expectVectorConformanceCompileReject(makeExperimentalVectorVariadicConstructorMismatchSource(),
                                         "experimental_vector_variadic_ctor_mismatch",
                                         emitMode,
                                         "/std/collections/vector/vector",
                                         "argument type mismatch");
    return;
  }

  expectVectorConformanceCompileReject(makeExperimentalVectorVariadicConstructorMismatchSource(),
                                       "experimental_vector_variadic_ctor_mismatch",
                                       emitMode,
                                       "collection literal requires element type i32");
}

inline std::string makeExperimentalVectorMoveOwnershipSource() {
  std::string source;
  source += "import /std/collections/vector/*\n\n";
  source += "[effects(heap_alloc), return<Vector<i32>>]\n";
  source += "wrapValues([i32] first, [i32] second) {\n";
  source += "  return(vector<i32>(first, second))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32> mut] first{wrapValues(2i32, 4i32)}\n";
  source += "  [Vector<i32>] second{move(first)}\n";
  source += "  assign(first, vector<i32>(9i32))\n";
  source += "  return(plus(plus(vectorCount<i32>(second), vectorAt<i32>(second, 1i32)),\n";
  source += "              plus(vectorCount<i32>(first), vectorAt<i32>(first, 0i32))))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorMoveOwnershipConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorMoveOwnershipSource(),
                                     "experimental_vector_move_ownership_" + emitMode,
                                     emitMode,
                                     16);
}

inline std::string makeExperimentalVectorCountRefConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(3i32, 5i32, 7i32)}\n";
  source += "  [Reference<Vector<i32>>] borrowed{location(values)}\n";
  source += "  return(vectorCountRef<i32>(borrowed))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorCountRefConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorCountRefConformanceSource(),
                                     "experimental_vector_count_ref_" + emitMode,
                                     emitMode,
                                     3);
}

inline std::string makeExperimentalVectorCapacityRefConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(3i32, 6i32, 9i32)}\n";
  source += "  [Reference<Vector<i32>>] borrowed{location(values)}\n";
  source += "  return(vectorCapacityRef<i32>(borrowed))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorCapacityRefConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorCapacityRefConformanceSource(),
                                     "experimental_vector_capacity_ref_" + emitMode,
                                     emitMode,
                                     3);
}

inline std::string makeExperimentalVectorIsEmptyRefConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(3i32)}\n";
  source += "  [Vector<i32>] empty{vector<i32>()}\n";
  source += "  [Reference<Vector<i32>>] borrowedValues{location(values)}\n";
  source += "  [Reference<Vector<i32>>] borrowedEmpty{location(empty)}\n";
  source += "  [i32 mut] total{0i32}\n";
  source += "  if(vectorIsEmptyRef<i32>(borrowedValues),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { assign(total, plus(total, 2i32)) })\n";
  source += "  if(vectorIsEmptyRef<i32>(borrowedEmpty),\n";
  source += "     then() { assign(total, plus(total, 4i32)) },\n";
  source += "     else() { assign(total, plus(total, 8i32)) })\n";
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorIsEmptyRefConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorIsEmptyRefConformanceSource(),
                                     "experimental_vector_is_empty_ref_" + emitMode,
                                     emitMode,
                                     6);
}

inline std::string makeExperimentalVectorCountRefAfterPushConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(1i32)}\n";
  source += "  [Reference<Vector<i32>>] borrowed{location(values)}\n";
  source += "  vectorPush<i32>(values, 5i32)\n";
  source += "  return(vectorCountRef<i32>(borrowed))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorCountRefAfterPushConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorCountRefAfterPushConformanceSource(),
                                     "experimental_vector_count_ref_after_push_" + emitMode,
                                     emitMode,
                                     2);
}

inline std::string makeExperimentalVectorCapacityRefAfterReserveConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(1i32)}\n";
  source += "  [Reference<Vector<i32>>] borrowed{location(values)}\n";
  source += "  vectorReserve<i32>(values, 10i32)\n";
  source += "  return(vectorCapacityRef<i32>(borrowed))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalVectorCapacityRefAfterReserveConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorCapacityRefAfterReserveConformanceSource(),
                                     "experimental_vector_capacity_ref_after_reserve_" + emitMode,
                                     emitMode,
                                     10);
}

inline std::string makeVectorIndexedRemovalOwnershipConformanceSource(const std::string &mode,
                                                                      bool validateWithInternalTake = false) {
  std::string source;
  source += "import /std/collections/*\n\n";
  if (validateWithInternalTake) {
    source += "import /std/collections/vector/*\n\n";
  }
  if (mode == "remove_at_drop") {
    source += "[struct]\n";
    source += "Owned() {\n";
    source += "  [i32] value{4i32}\n\n";
    source += "  Destroy() {\n";
    source += "  }\n";
    source += "}\n\n";
    source += "[effects(heap_alloc), return<int>]\n";
    source += "main() {\n";
    source += "  [vector<Owned> mut] values{vector<Owned>()}\n";
    source += "  /std/collections/vector/push<Owned>(values, Owned())\n";
    source += "  /std/collections/vector/push<Owned>(values, Owned(9i32))\n";
    source += "  /std/collections/vector/remove_at<Owned>(values, 0i32)\n";
    if (validateWithInternalTake) {
      source += "  [Owned] survivor{/std/collections/vector/vectorTakeSlot<Owned>(values, 0i32)}\n";
      source += "  return(plus(/std/collections/vector/count<Owned>(values), survivor.value))\n";
    } else {
      source += "  return(plus(/std/collections/vector/count<Owned>(values),\n";
      source += "              /std/collections/vector/at<Owned>(values, 0i32).value))\n";
    }
    source += "}\n";
    return source;
  }

  source += "[struct]\n";
  source += "Mover() {\n";
  source += "  [i32] value{1i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this, other)\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[struct]\n";
  source += "Wrapper() {\n";
  source += "  [Mover] value{Mover()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<Wrapper> mut] values{vector<Wrapper>()}\n";
  source += "  /std/collections/vector/push<Wrapper>(values, Wrapper(Mover(1i32)))\n";
  source += "  /std/collections/vector/push<Wrapper>(values, Wrapper(Mover(7i32)))\n";
  source += "  /std/collections/vector/remove_swap<Wrapper>(values, 0i32)\n";
  if (validateWithInternalTake) {
    source += "  [Wrapper] survivor{/std/collections/vector/vectorTakeSlot<Wrapper>(values, 0i32)}\n";
    source += "  return(plus(/std/collections/vector/count<Wrapper>(values), survivor.value.value))\n";
  } else {
    source += "  return(plus(/std/collections/vector/count<Wrapper>(values),\n";
    source += "              /std/collections/vector/at_unsafe<Wrapper>(values, 0i32).value.value))\n";
  }
  source += "}\n";
  return source;
}

inline void expectVectorIndexedRemovalOwnershipConformance(const std::string &emitMode,
                                                           const std::string &mode,
                                                           int expectedOut) {
  // TODO-4740 (fixed): the "remove_swap_relocation" mode's struct-of-struct
  // push payload (Wrapper(Mover(...))) used to under-allocate its vector's
  // backing heap buffer on both the vm and exe backends (the same root
  // cause as expectCanonicalVectorIndexedRemovalOwnershipConformance
  // above); both now compile and run correctly and return the
  // independently re-derived expectedOut. The "remove_at_drop" mode's
  // crash is a distinct, still-open bug (a struct value returned directly
  // from at()/at_unsafe() and immediately field-accessed - TODO-4804's
  // territory), unaffected by this fix, so it keeps its crash
  // expectation.
  if (mode == "remove_at_drop" && emitMode == "vm") {
    expectVectorConformanceCompileReject(
        makeVectorIndexedRemovalOwnershipConformanceSource(mode),
        "vector_indexed_removal_ownership_" + mode + "_" + emitMode,
        emitMode,
        "VM error: unaligned indirect address in IR",
        "",
        3);
    return;
  }
  if (mode == "remove_at_drop" && emitMode == "exe") {
    const std::string source = makeVectorIndexedRemovalOwnershipConformanceSource(mode);
    const std::string nameStem = "vector_indexed_removal_ownership_" + mode + "_" + emitMode;
    const std::string srcPath = writeTemp(nameStem + ".prime", source);
    const std::string exePath = (testScratchPath("") / (nameStem + "_bin")).string();
    const std::string compileCmd =
        "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    const std::string runOutPath = (testScratchPath("") / (nameStem + "_run.txt")).string();
    CHECK(runCommand(quoteShellArg(exePath) + " > " + quoteShellArg(runOutPath) + " 2>&1") == 1);
    CHECK(readFile(runOutPath).find("unaligned indirect address in IR") != std::string::npos);
    return;
  }
  if (emitMode == "vm") {
    expectVectorConformanceProgramRuns(makeVectorIndexedRemovalOwnershipConformanceSource(mode),
                                       "vector_indexed_removal_ownership_" + mode + "_" + emitMode,
                                       emitMode,
                                       expectedOut);
    return;
  }
  if (emitMode == "exe") {
    expectVectorConformanceProgramRuns(makeVectorIndexedRemovalOwnershipConformanceSource(mode),
                                       "vector_indexed_removal_ownership_" + mode + "_" + emitMode,
                                       emitMode,
                                       expectedOut);
    return;
  }
  expectVectorConformanceProgramRuns(makeVectorIndexedRemovalOwnershipConformanceSource(mode, true),
                                     "vector_indexed_removal_ownership_" + mode + "_" + emitMode,
                                     emitMode,
                                     expectedOut);
}

inline std::string makeVectorPopEmptyRuntimeContractSource(bool methodForm) {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>()}\n";
  source += methodForm ? "  values.pop()\n" : "  pop(values)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline void expectVectorPopEmptyRuntimeContract(const std::string &emitMode,
                                                bool methodForm) {
  const std::string formSlug = methodForm ? "method" : "call";
  const std::string source = makeVectorPopEmptyRuntimeContractSource(methodForm);
  const std::string srcPath = writeTemp("vector_pop_empty_runtime_" + formSlug + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
                                                 "_err.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == "container empty\n");
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
                                                 "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "container empty\n");
}

inline std::string makeVectorIndexRuntimeContractSource(const std::string &mode) {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  const bool mutating = mode == "remove_at_call" || mode == "remove_at_method" ||
                        mode == "remove_swap_call" || mode == "remove_swap_method";
  source += mutating ? "  [vector<i32> mut] values{vector<i32>(4i32)}\n"
                     : "  [vector<i32>] values{vector<i32>(4i32)}\n";
  if (mode == "access_call") {
    source += "  return(at(values, 9i32))\n";
  } else if (mode == "access_method") {
    source += "  return(values.at(9i32))\n";
  } else if (mode == "access_bracket") {
    source += "  return(values[9i32])\n";
  } else if (mode == "remove_at_call") {
    source += "  remove_at(values, 1i32)\n";
    source += "  return(0i32)\n";
  } else if (mode == "remove_at_method") {
    source += "  values.remove_at(1i32)\n";
    source += "  return(0i32)\n";
  } else if (mode == "remove_swap_call") {
    source += "  remove_swap(values, 1i32)\n";
    source += "  return(0i32)\n";
  } else {
    source += "  values.remove_swap(1i32)\n";
    source += "  return(0i32)\n";
  }
  source += "}\n";
  return source;
}

inline void expectVectorIndexRuntimeContract(const std::string &emitMode,
                                             const std::string &mode) {
  const std::string source = makeVectorIndexRuntimeContractSource(mode);
  const std::string srcPath = writeTemp("vector_index_runtime_" + mode + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / ("primec_vector_index_runtime_" + mode + "_" + emitMode +
                                                 "_err.txt"))
          .string();
  // Both vm and exe now report all out-of-bounds vector access as "array
  // index out of bounds" uniformly (the "container index out of bounds"
  // wording used to be distinct for mutator-triggered bounds checks).
  const std::string expectedError = "array index out of bounds\n";

  // .remove_at(idx)/.remove_swap(idx) method-call sugar on a vector fails
  // to compile on both vm and exe (the bare-call form works fine on both) -
  // a genuine, separate gap; see TODO-4749's sibling investigation.
  const bool methodMutatorMode = mode == "remove_at_method" || mode == "remove_swap_method";
  if (methodMutatorMode) {
    const std::string helperName = mode == "remove_at_method" ? "remove_at" : "remove_swap";
    if (emitMode == "vm") {
      const std::string runCmd =
          "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
      CHECK(runCommand(runCmd) == 2);
      CHECK(readFile(errPath).find("missing semantic-product method-call target: " + helperName) !=
            std::string::npos);
      return;
    }
    const std::string discardExePath =
        (testScratchPath("") / ("primec_vector_index_runtime_" + mode + "_" + emitMode + "_discard_exe"))
            .string();
    const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                   quoteShellArg(discardExePath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(compileCmd) == 2);
    CHECK(readFile(errPath).find("missing semantic-product method-call target: " + helperName) !=
          std::string::npos);
    return;
  }

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (testScratchPath("") / ("primec_vector_index_runtime_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == expectedError);
}
