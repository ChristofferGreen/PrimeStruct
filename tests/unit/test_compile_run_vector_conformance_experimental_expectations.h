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
                                         "/std/collections/experimental_vector/vector",
                                         "argument type mismatch");
    return;
  }

  expectVectorConformanceCompileReject(makeExperimentalVectorVariadicConstructorMismatchSource(),
                                       "experimental_vector_variadic_ctor_mismatch",
                                       emitMode,
                                       "vector literal requires element type i32");
}

inline std::string makeExperimentalVectorMoveOwnershipSource() {
  std::string source;
  source += "import /std/collections/experimental_vector/*\n\n";
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
  source += "import /std/collections/experimental_vector/*\n";
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
  source += "import /std/collections/experimental_vector/*\n";
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
  source += "import /std/collections/experimental_vector/*\n";
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
  source += "import /std/collections/experimental_vector/*\n";
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
  source += "import /std/collections/experimental_vector/*\n";
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

inline std::string makeVectorIndexedRemovalOwnershipConformanceSource(const std::string &mode) {
  std::string source;
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
    source += "  push(values, Owned())\n";
    source += "  push(values, Owned(9i32))\n";
    source += "  remove_at(values, 0i32)\n";
    source += "  return(plus(count(values), at(values, 0i32).value))\n";
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
  source += "  push(values, Wrapper(Mover(1i32)))\n";
  source += "  push(values, Wrapper(Mover(7i32)))\n";
  source += "  remove_swap(values, 0i32)\n";
  source += "  return(plus(count(values), at_unsafe(values, 0i32).value.value))\n";
  source += "}\n";
  return source;
}

inline void expectVectorIndexedRemovalOwnershipConformance(const std::string &emitMode,
                                                           const std::string &mode,
                                                           int expectedOut) {
  if (emitMode == "vm" || emitMode == "exe") {
    expectVectorConformanceCompileReject(
        makeVectorIndexedRemovalOwnershipConformanceSource(mode),
        "vector_indexed_removal_ownership_" + mode + "_" + emitMode,
        emitMode,
        "/std/collections/vector/push");
    return;
  }
  expectVectorConformanceProgramRuns(makeVectorIndexedRemovalOwnershipConformanceSource(mode),
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
  const bool accessMode =
      mode == "access_call" || mode == "access_method" || mode == "access_bracket";
  const std::string expectedError =
      accessMode ? "array index out of bounds\n" : "container index out of bounds\n";

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
