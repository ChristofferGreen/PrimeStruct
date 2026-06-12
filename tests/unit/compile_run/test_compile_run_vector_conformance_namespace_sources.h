#pragma once

inline std::string makeCanonicalVectorNamespaceNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto] values{/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)}\n";
  source += "  return(plus(/std/collections/vector/count<i32>(values),\n";
  source += "      plus(/std/collections/vector/at<i32>(values, 0i32),\n";
  source += "          /std/collections/vector/at_unsafe<i32>(values, 1i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceNamedArgsTemporaryReceiverSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [i32] callCount{/std/collections/vector/count(/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32))}\n";
  source += "  [i32] methodCount{/std/collections/vector/vector<i32>([second] 8i32, [first] 7i32).count()}\n";
  source += "  [i32] tail{/std/collections/vector/vector<i32>([second] 12i32, [first] 11i32).at_unsafe(1i32)}\n";
  source += "  return(plus(plus(callCount, methodCount), tail))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceTypeMismatchRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto] values{/std/collections/vector/vector<i32>(1i32, false)}\n";
  source += "  return(/std/collections/vector/count<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceNamedArgsExplicitBindingRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32>] values{/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)}\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [auto] values{/std/collections/vector/vector<i32>(4i32, 5i32)}\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceExplicitBindingRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
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

inline std::string makeCanonicalVectorMutatorBoolRejectSource(const std::string &statementText) {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32, 6i32)}\n";
  source += "  " + statementText + "\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorReserveReceiverRejectSource(const std::string &statementText) {
  std::string source;
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [array<i32> mut] values{array<i32>(4i32, 5i32)}\n";
  source += "  " + statementText + "\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorPushReceiverRejectSource(const std::string &statementText) {
  std::string source;
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [array<i32> mut] values{array<i32>(4i32, 5i32)}\n";
  source += "  " + statementText + "\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorMutatorNamedArgExpressionRejectSource(const std::string &exprText) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}\n";
  source += "  return(" + exprText + ")\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorMutatorNamedArgReceiverRejectSource(const std::string &statementText) {
  std::string source;
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [array<i32> mut] values{array<i32>(4i32, 5i32)}\n";
  source += "  " + statementText + "\n";
  source += "  return(0i32)\n";
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
  source += "  [vector<i32>] values{vector<i32>(1i32, 2i32)}\n";
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
  source += "  [vector<i32> mut] values{vector<i32>(1i32)}\n";
  source += "  /std/collections/vector/push(values, 5i32)\n";
  source += "  return(plus(count(values), at(values, 0i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalVectorNamespaceTemporaryReceiverSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [i32] callCount{/std/collections/vector/count(/std/collections/vector/vector<i32>(4i32, 5i32, 6i32))}\n";
  source += "  [i32] methodCount{/std/collections/vector/vector<i32>(7i32, 8i32).count()}\n";
  source += "  [i32] tail{/std/collections/vector/vector<i32>(9i32, 10i32).at_unsafe(1i32)}\n";
  source += "  return(plus(plus(callCount, methodCount), tail))\n";
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
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>()}\n";
    source += "  /std/collections/vector/pop<i32>(values)\n";
  } else if (mode == "remove_at_oob") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32)}\n";
    source += "  /std/collections/vector/remove_at<i32>(values, 1i32)\n";
  } else if (mode == "at_negative_index") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32, 9i32)}\n";
    source += "  return(/std/collections/vector/at<i32>(values, -1i32))\n";
  } else if (mode == "at_unsafe_negative_index") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32, 9i32)}\n";
    source += "  return(/std/collections/vector/at_unsafe<i32>(values, -1i32))\n";
  } else if (mode == "push_growth_overflow") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>()}\n";
    source += "  values.set_field_count(1073741823i32)\n";
    source += "  values.set_field_capacity(1073741823i32)\n";
    source += "  /std/collections/vector/push<i32>(values, 3i32)\n";
  } else if (mode == "reserve_negative") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32, 9i32)}\n";
    source += "  /std/collections/vector/reserve<i32>(values, -1i32)\n";
  } else if (mode == "reserve_growth_overflow") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32, 9i32)}\n";
    source += "  /std/collections/vector/reserve<i32>(values, 1073741824i32)\n";
  } else {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{/std/collections/vector/vector<i32>(4i32)}\n";
    source += "  /std/collections/vector/remove_swap<i32>(values, 1i32)\n";
  }
  if (mode != "at_negative_index" && mode != "at_unsafe_negative_index") {
    source += "  return(0i32)\n";
  }
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorOwnedDropConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n\n";
  source += "[struct]\n";
  source += "Tracked() {\n";
  source += "  [i32 mut] value{0i32}\n";
  source += "  [bool mut] armed{true}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(this.armed, other.armed)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "    assign(other.armed, false)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<void>]\n";
  source += "destroy_pair() {\n";
  source += "  [Vector<Tracked>] values{/std/collections/vector/vector<Tracked>(Tracked(1i32, true), Tracked(2i32, true))}\n";
  source += "  }\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  destroy_pair()\n";
  source += "  [Vector<Tracked> mut] popped{/std/collections/vector/vector<Tracked>(Tracked(3i32, true))}\n";
  source += "  /std/collections/vector/pop<Tracked>(popped)\n";
  source += "  [Vector<Tracked> mut] cleared{/std/collections/vector/vector<Tracked>(Tracked(4i32, true), Tracked(5i32, true))}\n";
  source += "  /std/collections/vector/clear<Tracked>(cleared)\n";
  source += "  return(plus(/std/collections/vector/count<Tracked>(popped), /std/collections/vector/count<Tracked>(cleared)))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorRelocationConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n\n";
  source += "[struct]\n";
  source += "Mover() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<Mover> mut] values{/std/collections/vector/vector<Mover>()}\n";
  source += "  /std/collections/vector/push<Mover>(values, Mover(11i32))\n";
  source += "  /std/collections/vector/reserve<Mover>(values, 4i32)\n";
  source += "  /std/collections/vector/push<Mover>(values, Mover(22i32))\n";
  source += "  /std/collections/vector/push<Mover>(values, Mover(33i32))\n";
  source += "  return(plus(/std/collections/vector/capacity<Mover>(values),\n";
  source += "      plus(/std/collections/vector/count<Mover>(values),\n";
  source += "          plus(/std/collections/vector/at<Mover>(values, 0i32).value,\n";
  source += "              plus(/std/collections/vector/at_unsafe<Mover>(values, 1i32).value,\n";
  source += "                   /std/collections/vector/at<Mover>(values, 2i32).value)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorRemovalConformanceSource() {
  std::string source;
  source += "import /std/collections/vector/*\n\n";
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<Owned> mut] values{/std/collections/vector/vector<Owned>(Owned(10i32), Owned(20i32), Owned(30i32), Owned(40i32))}\n";
  source += "  /std/collections/vector/remove_at<Owned>(values, 1i32)\n";
  source += "  [i32] afterRemoveAt{plus(/std/collections/vector/count<Owned>(values),\n";
  source += "      plus(/std/collections/vector/at<Owned>(values, 0i32).value,\n";
  source += "          plus(/std/collections/vector/at_unsafe<Owned>(values, 1i32).value,\n";
  source += "               /std/collections/vector/at<Owned>(values, 2i32).value)))}\n";
  source += "  /std/collections/vector/remove_swap<Owned>(values, 0i32)\n";
  source += "  return(plus(afterRemoveAt,\n";
  source += "      plus(/std/collections/vector/count<Owned>(values),\n";
  source += "          plus(/std/collections/vector/at<Owned>(values, 0i32).value,\n";
  source += "               /std/collections/vector/at_unsafe<Owned>(values, 1i32).value))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalVectorCanonicalHelperRoutingSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/vector/*\n\n";
  source += "[struct]\n";
  source += "Mover() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<Vector<Mover>>]\n";
  source += "wrapValues() {\n";
  source += "  return(/std/collections/vector/vector<Mover>(Mover(10i32), Mover(20i32)))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Vector<Mover> mut] values{wrapValues()}\n";
  source += "  /std/collections/vector/push<Mover>(values, Mover(30i32))\n";
  source += "  /std/collections/vector/reserve<Mover>(values, 6i32)\n";
  source += "  [i32 mut] total{/std/collections/vector/count<Mover>(values)}\n";
  source += "  assign(total, plus(total, /std/collections/vector/capacity<Mover>(values)))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at<Mover>(values, 0i32).value))\n";
  source += "  assign(total, plus(total, /std/collections/vector/at_unsafe<Mover>(values, 2i32).value))\n";
  source += "  /std/collections/vector/push<Mover>(values, Mover(40i32))\n";
  source += "  assign(total, plus(total, /std/collections/vector/count<Mover>(values)))\n";
  source += "  assign(total, plus(total, values.at(3i32).value))\n";
  source += "  /std/collections/vector/remove_at<Mover>(values, 1i32)\n";
  source += "  /std/collections/vector/remove_swap<Mover>(values, 0i32)\n";
  source += "  values.pop()\n";
  source += "  /std/collections/vector/clear<Mover>(values)\n";
  source += "  return(plus(total, values.count()))\n";
  source += "}\n";
  return source;
}
