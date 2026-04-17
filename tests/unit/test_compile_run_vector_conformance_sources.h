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

inline std::string makeCanonicalVectorDiscardOwnershipConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[struct]\n";
  source += "Wrapper() {\n";
  source += "  [Owned] value{Owned()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<Owned> mut] popped{/std/collections/vectorSingle<Owned>(Owned(3i32))}\n";
  source += "  popped.pop()\n";
  source += "  [Vector<Wrapper> mut] cleared{/std/collections/vectorPair<Wrapper>(Wrapper(Owned(4i32)), Wrapper(Owned(5i32)))}\n";
  source += "  /std/collections/vector/clear<Wrapper>(cleared)\n";
  source += "  return(plus(/std/collections/vector/count<Owned>(popped), /std/collections/vector/count<Wrapper>(cleared)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorIndexedRemovalOwnershipConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[struct]\n";
  source += "Wrapper() {\n";
  source += "  [Owned] value{Owned()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<Owned> mut] removed{/std/collections/vectorPair<Owned>(Owned(4i32), Owned(9i32))}\n";
  source += "  remove_at(removed, 0i32)\n";
  source += "  [Vector<Wrapper> mut] swapped{/std/collections/vectorTriple<Wrapper>(\n";
  source += "      Wrapper(Owned(1i32)),\n";
  source += "      Wrapper(Owned(7i32)),\n";
  source += "      Wrapper(Owned(11i32)))}\n";
  source += "  remove_swap(swapped, 0i32)\n";
  source += "  return(plus(\n";
  source += "      plus(/std/collections/vector/count<Owned>(removed), /std/collections/vector/at<Owned>(removed, 0i32).value),\n";
  source += "      plus(/std/collections/vector/count<Wrapper>(swapped),\n";
  source += "           /std/collections/vector/at_unsafe<Wrapper>(swapped, 0i32).value.value)))\n";
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
  source += "  [auto mut] grown{/std/collections/vector/vector<i32>()}\n";
  source += "  /std/collections/vector/reserve<i32>(grown, 2i32)\n";
  source += "  [i32] reserved{/std/collections/vector/capacity<i32>(grown)}\n";
  source += "  /std/collections/vector/push(grown, 11i32)\n";
  source += "  /std/collections/vector/push(grown, 22i32)\n";
  source += "  /std/collections/vector/push(grown, 33i32)\n";
  source += "  [i32 mut] total{plus(reserved, /std/collections/vector/count<i32>(grown))}\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(grown, 0i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(grown, 1i32)))\n";
  source += "  [auto mut] shaped{/std/collections/vector/vector<i32>(10i32, 20i32, 30i32, 40i32)}\n";
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

inline std::string makeCanonicalVectorNamespaceExplicitVectorBindingSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32> mut] grown{vectorNew<i32>()}\n";
  source += "  /std/collections/vector/reserve<i32>(grown, 2i32)\n";
  source += "  [i32] reserved{/std/collections/vector/capacity<i32>(grown)}\n";
  source += "  /std/collections/vector/push(grown, 11i32)\n";
  source += "  /std/collections/vector/push(grown, 22i32)\n";
  source += "  /std/collections/vector/push(grown, 33i32)\n";
  source += "  [i32 mut] total{plus(reserved, /std/collections/vector/count<i32>(grown))}\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(grown, 0i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(grown, 1i32)))\n";
  source += "  [Vector<i32> mut] shaped{vectorQuad<i32>(10i32, 20i32, 30i32, 40i32)}\n";
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

inline std::string makeStdlibWrapperVectorHelperExplicitVectorBindingSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}\n";
  source += "  vectorReserve<i32>(values, 6i32)\n";
  source += "  vectorPush<i32>(values, 6i32)\n";
  source += "  [i32 mut] total{plus(vectorCount<i32>(values), vectorCapacity<i32>(values))}\n";
  source += "  assign(total, plus(total, vectorAt<i32>(values, 0i32)))\n";
  source += "  assign(total, plus(total, vectorAtUnsafe<i32>(values, 2i32)))\n";
  source += "  vectorPop<i32>(values)\n";
  source += "  assign(total, plus(total, vectorCount<i32>(values)))\n";
  source += "  vectorRemoveAt<i32>(values, 0i32)\n";
  source += "  assign(total, plus(total, vectorAt<i32>(values, 0i32)))\n";
  source += "  vectorPush<i32>(values, 9i32)\n";
  source += "  vectorRemoveSwap<i32>(values, 0i32)\n";
  source += "  assign(total, plus(total, vectorAtUnsafe<i32>(values, 0i32)))\n";
  source += "  vectorClear<i32>(values)\n";
  source += "  return(plus(total, vectorCount<i32>(values)))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorHelperExplicitVectorBindingMismatchSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}\n";
  source += "  return(vectorCount<bool>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorExplicitVectorBindingSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[return<Vector<i32>> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source += "  return(/std/collections/vectorPair<i32>(2i32, 4i32))\n";
  source += "}\n\n";
  source += "[return<i32> effects(heap_alloc)]\n";
  source += "scoreValues([Vector<i32>] values) {\n";
  source += "  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/at_unsafe<i32>(values, 1i32)))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32> mut] direct{/std/collections/vectorPair<i32>(4i32, 5i32)}\n";
  source += "  [Vector<i32>] wrapped{wrapValues(/std/collections/vectorSingle<i32>(6i32))}\n";
  source += "  [Vector<i32> mut] assigned{/std/collections/vectorNew<i32>()}\n";
  source += "  assign(assigned, buildValues())\n";
  source += "  /std/collections/vector/push<i32>(direct, 7i32)\n";
  source += "  [i32 mut] total{scoreValues(/std/collections/vectorPair<i32>(1i32, 3i32))}\n";
  source += "  assign(total, plus(total, /std/collections/vector/count<i32>(direct)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(direct, 0i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(direct, 2i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<i32>(wrapped, 0i32)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/count<i32>(assigned)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(assigned, 1i32)))\n";
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorExplicitVectorBindingMismatchSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[return<Vector<i32>> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source += "  return(/std/collections/vectorPair<i32>(2i32, false))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<i32>] values{wrapValues(buildValues())}\n";
  source += "  return(/std/collections/vector/count<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorAutoInferenceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] wrapped) {\n";
  source += "  if(wrapped,\n";
  source += "    then() {\n";
  source += "      return(wrapValues(/std/collections/vectorPair(11i32, 13i32)))\n";
  source += "    },\n";
  source += "    else() {\n";
  source += "      return(/std/collections/vectorSingle(19i32))\n";
  source += "    })\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto mut] values{/std/collections/vectorNew<i32>()}\n";
  source += "  /std/collections/vector/push<i32>(values, 3i32)\n";
  source += "  /std/collections/vector/push<i32>(values, 5i32)\n";
  source += "  [auto mut] wrapped{buildValues(true)}\n";
  source += "  /std/collections/vector/push<i32>(wrapped, 17i32)\n";
  source += "  [auto] single{buildValues(false)}\n";
  source += "  return(plus(/std/collections/vector/count<i32>(values),\n";
  source += "      plus(/std/collections/vector/at<i32>(values, 0i32),\n";
  source += "          plus(/std/collections/vector/count<i32>(wrapped),\n";
  source += "              plus(/std/collections/vector/at<i32>(wrapped, 1i32),\n";
  source += "                  plus(/std/collections/vector/at_unsafe<i32>(wrapped, 2i32),\n";
  source += "                      /std/collections/vector/at<i32>(single, 0i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makePortableStdlibVectorConstructorAutoInferenceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] wrapped) {\n";
  source += "  if(wrapped,\n";
  source += "    then() {\n";
  source += "      return(/std/collections/vectorPair(11i32, 13i32))\n";
  source += "    },\n";
  source += "    else() {\n";
  source += "      return(/std/collections/vectorSingle(19i32))\n";
  source += "    })\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto mut] wrapped{buildValues(true)}\n";
  source += "  /std/collections/vector/push<i32>(wrapped, 17i32)\n";
  source += "  [auto] single{buildValues(false)}\n";
  source += "  return(plus(/std/collections/vector/count<i32>(wrapped),\n";
  source += "      plus(/std/collections/vector/at<i32>(wrapped, 1i32),\n";
  source += "          plus(/std/collections/vector/at_unsafe<i32>(wrapped, 2i32),\n";
  source += "              /std/collections/vector/at<i32>(single, 0i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorAutoInferenceMismatchSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto] values{wrapValues(/std/collections/vectorPair(2i32, false))}\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [i32] directHelperCount{/std/collections/vector/count(/std/collections/vectorPair(11i32, 13i32))}\n";
  source += "  [i32] directHelperValue{/std/collections/vector/at(/std/collections/vectorPair(17i32, 19i32), 1i32)}\n";
  source += "  [i32] wrappedHelperValue{vectorAt<i32>(wrapValues(/std/collections/vectorPair(23i32, 29i32)), 0i32)}\n";
  source += "  [i32] directMethodCount{/std/collections/vectorPair(31i32, 37i32).count()}\n";
  source += "  [i32] directMethodValue{/std/collections/vectorPair(41i32, 43i32).at_unsafe(1i32)}\n";
  source += "  [i32] wrappedMethodValue{wrapValues(/std/collections/vectorPair(47i32, 53i32)).at(0i32)}\n";
  source += "  return(plus(plus(directHelperCount, directHelperValue),\n";
  source += "      plus(plus(wrappedHelperValue, directMethodCount),\n";
  source += "          plus(directMethodValue, wrappedMethodValue))))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorHelperReceiverMismatchSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  return(/std/collections/vector/count(/std/collections/vectorPair(2i32, false)))\n";
  source += "}\n";
  return source;
}

inline std::string makeStdlibWrapperVectorConstructorMethodReceiverMismatchSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_vector/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  return(/std/collections/vectorPair(2i32, false).count())\n";
  source += "}\n";
  return source;
}
