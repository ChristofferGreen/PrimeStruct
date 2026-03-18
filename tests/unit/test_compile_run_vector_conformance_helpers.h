#pragma once

inline std::string vectorHelperConformanceImportSlug(const std::string &importPath) {
  if (importPath == "/std/collections/*") {
    return "stdlib";
  }
  if (importPath == "/std/collections/experimental_vector/*") {
    return "experimental";
  }
  return "custom";
}

inline bool isExperimentalVectorImport(const std::string &importPath) {
  return importPath == "/std/collections/experimental_vector/*";
}

inline std::string vectorConformanceType(const std::string &importPath,
                                         const std::string &valueType) {
  if (isExperimentalVectorImport(importPath)) {
    return "Vector<" + valueType + ">";
  }
  return "vector<" + valueType + ">";
}

inline std::string makeVectorHelperSurfaceConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<" + vectorConformanceType(importPath, "T") + ">]\n";
  source += "wrapVector<T>([T] first, [T] second, [T] third, [T] fourth) {\n";
  source += "  return(vectorQuad<T>(first, second, third, fourth))\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] empty{vectorNew<i32>()}\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] pair{vectorPair<i32>(11i32, 13i32)}\n";
  source += "  [" + vectorConformanceType(importPath, "i32") +
            "] wrapped{wrapVector<i32>(1i32, 3i32, 5i32, 7i32)}\n";
  source += "  print_line(vectorCount<i32>(empty))\n";
  source += "  print_line(vectorCount<i32>(pair))\n";
  source += "  print_line(vectorCapacity<i32>(pair))\n";
  source += "  print_line(vectorAt<i32>(pair, 1i32))\n";
  source += "  print_line(vectorAtUnsafe<i32>(pair, 0i32))\n";
  source += "  print_line(vectorAt<i32>(wrapped, 2i32))\n";
  source += "  print_line(vectorAtUnsafe<i32>(wrapped, 3i32))\n";
  source += "  return(plus(plus(vectorCount<i32>(empty), vectorCount<i32>(pair)),\n";
  source += "      plus(vectorCapacity<i32>(pair), plus(vectorAt<i32>(pair, 1i32),\n";
  source += "          plus(vectorAtUnsafe<i32>(pair, 0i32), plus(vectorAt<i32>(wrapped, 2i32),\n";
  source += "              vectorAtUnsafe<i32>(wrapped, 3i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorExtendedConstructorConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") +
            "] values{vectorOct<i32>(2i32, 4i32, 6i32, 8i32, 10i32, 12i32, 14i32, 16i32)}\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  print_line(vectorCapacity<i32>(values))\n";
  source += "  print_line(vectorAt<i32>(values, 6i32))\n";
  source += "  print_line(vectorAtUnsafe<i32>(values, 0i32))\n";
  source += "  return(plus(plus(vectorCount<i32>(values), vectorCapacity<i32>(values)),\n";
  source += "      plus(vectorAt<i32>(values, 6i32), vectorAtUnsafe<i32>(values, 0i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorGrowthConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
  source += "  vectorReserve<i32>(values, 2i32)\n";
  source += "  [i32] reserved{vectorCapacity<i32>(values)}\n";
  source += "  vectorPush<i32>(values, 11i32)\n";
  source += "  vectorPush<i32>(values, 22i32)\n";
  source += "  vectorPush<i32>(values, 33i32)\n";
  source += "  print_line(reserved)\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  print_line(vectorAt<i32>(values, 0i32))\n";
  source += "  print_line(vectorAtUnsafe<i32>(values, 1i32))\n";
  source += "  print_line(vectorAt<i32>(values, 2i32))\n";
  source += "  return(plus(plus(reserved, vectorCount<i32>(values)),\n";
  source += "      plus(vectorAt<i32>(values, 0i32), plus(vectorAtUnsafe<i32>(values, 1i32),\n";
  source += "          vectorAt<i32>(values, 2i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorShrinkRemoveConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorQuad<i32>(10i32, 20i32, 30i32, 40i32)}\n";
  source += "  [i32 mut] total{0i32}\n";
  source += "  vectorPop<i32>(values)\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  print_line(vectorAt<i32>(values, 1i32))\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAt<i32>(values, 1i32))))\n";
  source += "  vectorRemoveAt<i32>(values, 1i32)\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  print_line(vectorAtUnsafe<i32>(values, 1i32))\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAtUnsafe<i32>(values, 1i32))))\n";
  source += "  vectorRemoveSwap<i32>(values, 0i32)\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  print_line(vectorAt<i32>(values, 0i32))\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAt<i32>(values, 0i32))))\n";
  source += "  vectorClear<i32>(values)\n";
  source += "  print_line(vectorCount<i32>(values))\n";
  source += "  return(plus(total, vectorCount<i32>(values)))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] values{vectorPair<i32>(1i32, false)}\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorVariadicConstructorSource() {
  std::string source;
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<Vector<T>>]\n";
  source += "wrapVector<T>([T] first, [T] second, [T] third) {\n";
  source += "  return(vector<T>(first, second, third))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] empty{vector<i32>()}\n";
  source += "  [Vector<i32>] direct{vector<i32>(2i32, 4i32, 6i32, 8i32)}\n";
  source += "  [Vector<i32>] wrapped{wrapVector<i32>(3i32, 5i32, 7i32)}\n";
  source += "  return(plus(plus(vectorCount<i32>(empty), vectorCount<i32>(direct)),\n";
  source += "      plus(vectorAt<i32>(direct, 2i32), plus(vectorAtUnsafe<i32>(wrapped, 1i32), vectorCount<i32>(wrapped)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorVariadicConstructorMismatchSource() {
  std::string source;
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{vector<i32>(1i32, false)}\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorPopTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(9i32)}\n";
  source += "  vectorPop<bool>(values)\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorPushTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
  source += "  vectorPush<bool>(values, true)\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] grown{/std/collections/vector/vector<i32>()}\n";
  source += "  /std/collections/vector/reserve<i32>(grown, 2i32)\n";
  source += "  [i32] reserved{/std/collections/vector/capacity<i32>(grown)}\n";
  source += "  /std/collections/vector/push(grown, 11i32)\n";
  source += "  /std/collections/vector/push(grown, 22i32)\n";
  source += "  /std/collections/vector/push(grown, 33i32)\n";
  source += "  [i32 mut] total{plus(reserved, /std/collections/vector/count<i32>(grown))}\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(grown, 0i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(grown, 1i32)))\n";
  source +=
      "  [vector<i32> mut] shaped{/std/collections/vector/vector<i32>(10i32, 20i32, 30i32, 40i32)}\n";
  source += "  /std/collections/vector/remove_at<i32>(shaped, 1i32)\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(shaped, 1i32)))\n";
  source += "  /std/collections/vector/remove_swap<i32>(shaped, 0i32)\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(shaped, 0i32)))\n";
  source += "  /std/collections/vector/pop<i32>(shaped)\n";
  source += "  assign(total, plus(total, /std/collections/vector/count<i32>(shaped)))\n";
  source += "  /std/collections/vector/clear<i32>(shaped)\n";
  source += "  return(plus(total, /std/collections/vector/count<i32>(shaped)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)}\n";
  source += "  return(plus(/std/collections/vector/count<i32>(values),\n";
  source += "      plus(/std/collections/vector/at<i32>(values, 0i32),\n";
  source += "          /std/collections/vector/at_unsafe<i32>(values, 1i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceTypeMismatchRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{/std/collections/vector/vector<i32>(1i32, false)}\n";
  source += "  return(/std/collections/vector/count<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorCountNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(/std/collections/vector/count([values] values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorCountImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(/std/collections/vector/count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorCapacityNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(/std/collections/vector/capacity([values] values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorCapacityImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(/std/collections/vector/capacity(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorAccessNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(plus(/std/collections/vector/at([index] 0i32, [values] values),\n";
  source += "      /std/collections/vector/at_unsafe([index] 1i32, [values] values)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorAccessImportRequirementSource(const std::string &helperName) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(/std/collections/vector/" + helperName + "(values, 0i32))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorPushNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32)}\n";
  source += "  /std/collections/vector/push([value] 5i32, [values] values)\n";
  source += "  return(/std/collections/vector/count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorPushImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32)}\n";
  source += "  /std/collections/vector/push(values, 5i32)\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorPopNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}\n";
  source += "  /std/collections/vector/pop([values] values)\n";
  source += "  return(/std/collections/vector/count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorPopImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}\n";
  source += "  /std/collections/vector/pop(values)\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorReserveNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}\n";
  source += "  /std/collections/vector/reserve([capacity] 8i32, [values] values)\n";
  source += "  return(plus(/std/collections/vector/count(values), /std/collections/vector/capacity(values)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorReserveImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}\n";
  source += "  /std/collections/vector/reserve(values, 8i32)\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorMutatorImportRequirementSource(const std::string &helperName,
                                                                     const std::string &callArgs) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32, 6i32)}\n";
  source += "  /std/collections/vector/" + helperName + "(" + callArgs + ")\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeBareVectorMutatorImportRequirementSource(const std::string &helperName,
                                                                const std::string &callArgs) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32, 6i32)}\n";
  source += "  " + helperName + "(" + callArgs + ")\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeBareVectorMutatorMethodImportRequirementSource(const std::string &helperName,
                                                                      const std::string &methodArgs) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32, 6i32)}\n";
  source += "  values." + helperName + "(" + methodArgs + ")\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceCountShadowSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[return<int>]\n";
  source += "/count([vector<i32>] values) {\n";
  source += "  return(91i32)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{/std/collections/vector/vector<i32>(1i32, 2i32)}\n";
  source += "  return(/std/collections/vector/count<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespacePushShadowSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<void>]\n";
  source += "/push([vector<i32> mut] values, [i32] value) {\n";
  source += "  assign(values, vector<i32>(97i32, value))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{/std/collections/vector/vector<i32>(1i32)}\n";
  source += "  /std/collections/vector/push(values, 5i32)\n";
  source += "  return(plus(count(values), at(values, 0i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorHelperRuntimeContractSource(const std::string &importPath,
                                                         const std::string &mode) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  if (mode == "pop_empty") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
    source += "  vectorPop<i32>(values)\n";
  } else if (mode == "remove_at_oob") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(4i32)}\n";
    source += "  vectorRemoveAt<i32>(values, 1i32)\n";
  } else {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(4i32)}\n";
    source += "  vectorRemoveSwap<i32>(values, 1i32)\n";
  }
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorOwnershipRejectSource(const std::string &mode) {
  std::string source;
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32] value{1i32}\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  if (mode == "constructor") {
    source += "  [Vector<Owned>] values{vectorSingle<Owned>(Owned())}\n";
    source += "  return(vectorCount<Owned>(values))\n";
  } else if (mode == "push") {
    source += "  [Vector<Owned> mut] values{vectorNew<Owned>()}\n";
    source += "  vectorPush<Owned>(values, Owned())\n";
    source += "  return(vectorCount<Owned>(values))\n";
  } else if (mode == "reserve") {
    source += "  [Vector<Owned> mut] values{vectorNew<Owned>()}\n";
    source += "  vectorReserve<Owned>(values, 4i32)\n";
    source += "  return(vectorCapacity<Owned>(values))\n";
  } else if (mode == "pop") {
    source += "  [Vector<Owned> mut] values{vectorSingle<Owned>(Owned())}\n";
    source += "  vectorPop<Owned>(values)\n";
    source += "  return(vectorCount<Owned>(values))\n";
  } else if (mode == "clear") {
    source += "  [Vector<Owned> mut] values{vectorSingle<Owned>(Owned())}\n";
    source += "  vectorClear<Owned>(values)\n";
    source += "  return(vectorCount<Owned>(values))\n";
  } else if (mode == "remove_at") {
    source += "  [Vector<Owned> mut] values{vectorPair<Owned>(Owned(), Owned())}\n";
    source += "  vectorRemoveAt<Owned>(values, 0i32)\n";
    source += "  return(vectorCount<Owned>(values))\n";
  } else {
    source += "  [Vector<Owned> mut] values{vectorPair<Owned>(Owned(), Owned())}\n";
    source += "  vectorRemoveSwap<Owned>(values, 0i32)\n";
    source += "  return(vectorCount<Owned>(values))\n";
  }
  source += "}\n";
  return source;
}

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
      (std::filesystem::temp_directory_path() / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectVectorVmProgramRunsWithOutput(const std::string &source,
                                                const std::string &nameStem,
                                                int expectedExitCode,
                                                const std::string &expectedOutput) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / (nameStem + "_vm_out.txt")).string();
  const std::string runCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedOutput);
}

inline void expectVectorConformanceCompileReject(const std::string &source,
                                                 const std::string &nameStem,
                                                 const std::string &emitMode,
                                                 const std::string &expectedFragment,
                                                 const std::string &requiredFragment = "") {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / (nameStem + "_" + emitMode + "_out.txt")).string();
  const std::string command = emitMode == "vm"
                                  ? "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                                        quoteShellArg(outPath) + " 2>&1"
                                  : "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                        " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(command) == 2);
  const std::string diagnostics = readFile(outPath);
  if (!requiredFragment.empty()) {
    CHECK(diagnostics.find(requiredFragment) != std::string::npos);
  }
  CHECK(diagnostics.find(expectedFragment) != std::string::npos);
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
                                       "/std/collections/vectorPop",
                                       "argument type mismatch");
}

inline void expectVectorPushTypeMismatchReject(const std::string &emitMode,
                                               const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceCompileReject(makeVectorPushTypeMismatchRejectSource(importPath),
                                       "vector_push_type_mismatch_" + slug,
                                       emitMode,
                                       "/std/collections/vectorPush",
                                       "argument type mismatch");
}

inline void expectCanonicalVectorNamespaceConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceConformanceSource(),
      "vector_namespace_canonical_" + emitMode,
      emitMode,
      109);
}

inline void expectCanonicalVectorNamespaceNamedArgsConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(
      makeCanonicalVectorNamespaceNamedArgsSource(),
      "vector_namespace_canonical_named_args_" + emitMode,
      emitMode,
      11);
}

inline void expectCanonicalVectorNamespaceTypeMismatchReject(const std::string &emitMode) {
  const std::string source = makeCanonicalVectorNamespaceTypeMismatchRejectSource();
  const std::string srcPath = writeTemp("vector_namespace_canonical_type_mismatch_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_namespace_canonical_type_mismatch_" + emitMode + "_out.txt"))
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

inline void expectCanonicalVectorNamespaceVmImportRequirement() {
  expectVectorConformanceCompileReject(makeCanonicalVectorNamespaceImportRequirementSource(),
                                       "vector_namespace_canonical_import_requirement",
                                       "vm",
                                       "/std/collections/vector/vector");
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

inline void expectCanonicalVectorMutatorImportRequirement(const std::string &emitMode,
                                                          const std::string &helperName,
                                                          const std::string &callArgs) {
  const std::string source = makeCanonicalVectorMutatorImportRequirementSource(helperName, callArgs);
  const std::string srcPath =
      writeTemp("vector_" + helperName + "_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
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

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
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
      (std::filesystem::temp_directory_path() /
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
    const std::string exePath =
        (std::filesystem::temp_directory_path() /
         ("primec_vector_bare_" + helperName + "_import_requirement_" + emitMode + "_exe"))
            .string();
    const std::string compileCmd =
        "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
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
      (std::filesystem::temp_directory_path() /
       ("primec_vector_bare_" + helperName + "_method_import_requirement_" + emitMode + "_out.txt"))
          .string();
  const std::string expected = emitMode == "native"
                                   ? "unknown call target: /std/collections/vector/" + helperName
                                   : "unknown method: /vector/" + helperName;

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  if (emitMode == "native" || emitMode == "exe") {
    const std::string exePath =
        (std::filesystem::temp_directory_path() /
         ("primec_vector_bare_" + helperName + "_method_import_requirement_" + emitMode + "_exe"))
            .string();
    const std::string compileCmd =
        "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
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

inline void expectVectorHelperRuntimeContract(const std::string &emitMode,
                                              const std::string &importPath,
                                              const std::string &mode) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorHelperRuntimeContractSource(importPath, mode);
  const std::string srcPath = writeTemp("vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_err.txt"))
          .string();
  const std::string expectedError = mode == "pop_empty" ? "container empty\n" : "container index out of bounds\n";

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == expectedError);
}

inline void expectExperimentalVectorOwnershipReject(const std::string &emitMode,
                                                    const std::string &mode,
                                                    const std::string &expectedError) {
  const std::string source = makeExperimentalVectorOwnershipRejectSource(mode);
  const std::string srcPath = writeTemp("experimental_vector_ownership_" + mode + "_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_experimental_vector_ownership_" + mode + "_" + emitMode + "_out.txt"))
          .string();

  const std::string command = emitMode == "vm"
                                  ? "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                                        quoteShellArg(outPath) + " 2>&1"
                                  : "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                        " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath).find(expectedError) != std::string::npos);
}

inline void expectExperimentalVectorVariadicConstructorConformance(const std::string &emitMode) {
  expectVectorConformanceProgramRuns(makeExperimentalVectorVariadicConstructorSource(),
                                     "experimental_vector_variadic_ctor_" + emitMode,
                                     emitMode,
                                     18);
}

inline void expectExperimentalVectorVariadicConstructorMismatchReject(const std::string &emitMode) {
  expectVectorConformanceCompileReject(makeExperimentalVectorVariadicConstructorMismatchSource(),
                                       "experimental_vector_variadic_ctor_mismatch",
                                       emitMode,
                                       "/std/collections/experimental_vector/vector",
                                       "argument type mismatch");
}

inline std::string makeVectorIndexedRemovalOwnershipRejectSource(const std::string &mode) {
  std::string source;
  if (mode == "remove_at_drop") {
    source += "[struct]\n";
    source += "Owned() {\n";
    source += "  [i32] value{1i32}\n\n";
    source += "  Destroy() {\n";
    source += "  }\n";
    source += "}\n\n";
    source += "[effects(heap_alloc), return<int>]\n";
    source += "main() {\n";
    source += "  [vector<Owned> mut] values{vector<Owned>(Owned(), Owned())}\n";
    source += "  remove_at(values, 0i32)\n";
    source += "  return(0i32)\n";
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
  source += "  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper(), Wrapper())}\n";
  source += "  remove_swap(values, 0i32)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline void expectVectorIndexedRemovalOwnershipReject(const std::string &emitMode,
                                                      const std::string &mode,
                                                      const std::string &expectedError) {
  const std::string source = makeVectorIndexedRemovalOwnershipRejectSource(mode);
  const std::string srcPath = writeTemp("vector_indexed_removal_ownership_" + mode + "_" + emitMode + ".prime",
                                        source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_indexed_removal_ownership_" + mode + "_" + emitMode + "_out.txt"))
          .string();

  const std::string command = emitMode == "vm"
                                  ? "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                                        quoteShellArg(outPath) + " 2>&1"
                                  : "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                        " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath).find(expectedError) != std::string::npos);
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
      (std::filesystem::temp_directory_path() / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
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
      (std::filesystem::temp_directory_path() / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
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
      (std::filesystem::temp_directory_path() / ("primec_vector_index_runtime_" + mode + "_" + emitMode +
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
      (std::filesystem::temp_directory_path() / ("primec_vector_index_runtime_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == expectedError);
}
