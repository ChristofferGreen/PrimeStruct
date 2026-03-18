#pragma once

#include "test_compile_run_map_conformance_helpers.h"
#include "test_compile_run_vector_conformance_helpers.h"

inline void expectVmSharedStdlibMapConformanceHarness() {
  SUBCASE("stdlib") {
    const std::string emitMode = "vm";
    expectVmStdlibMapHelperSurfaceConformance();
    expectVmStdlibMapExtendedConstructorConformance();
    expectMapTryAtConformance(emitMode, "/std/collections/*", false);
    expectMapTryAtConformance(emitMode, "/std/collections/*", true);
    expectCanonicalMapNamespaceVmConformance();
    expectCanonicalMapNamespaceNamedArgsVmConformance();
    expectCanonicalMapNamespaceTypeMismatchReject(emitMode);
    expectCanonicalMapNamespaceVmImportRequirement();
    expectCanonicalMapCountVmBuiltinRejectsTemplateArgs();
    expectCanonicalMapContainsVmImportRequirement();
    expectCanonicalMapTryAtVmImportRequirement();
    expectCanonicalMapAccessVmBuiltinConformance("at");
    expectCanonicalMapAccessVmBuiltinConformance("at_unsafe");
  }
}

inline void expectVmSharedStdlibVectorConformanceHarness() {
  SUBCASE("stdlib") {
    const std::string emitMode = "vm";
    expectVmStdlibVectorHelperSurfaceConformance();
    expectVmStdlibVectorExtendedConstructorConformance();
    expectVmStdlibVectorGrowthConformance();
    expectVmStdlibVectorShrinkRemoveConformance();
    expectVectorTypeMismatchReject(emitMode, "/std/collections/*");
    expectVectorPopTypeMismatchReject(emitMode, "/std/collections/*");
    expectVectorPushTypeMismatchReject(emitMode, "/std/collections/*");
    expectCanonicalVectorNamespaceNamedArgsConformance(emitMode);
    expectCanonicalVectorNamespaceTypeMismatchReject(emitMode);
    expectCanonicalVectorNamespaceVmImportRequirement();
    expectCanonicalVectorCountNamedArgsConformance(emitMode);
    expectCanonicalVectorCountVmImportRequirement();
    expectCanonicalVectorCapacityNamedArgsConformance(emitMode);
    expectCanonicalVectorCapacityVmImportRequirement();
    expectCanonicalVectorAccessNamedArgsConformance(emitMode);
    expectCanonicalVectorAccessVmImportRequirement("at");
    expectCanonicalVectorAccessVmImportRequirement("at_unsafe");
    expectCanonicalVectorPushNamedArgsConformance(emitMode);
    expectCanonicalVectorPushVmImportRequirement();
    expectCanonicalVectorPopNamedArgsConformance(emitMode);
    expectCanonicalVectorPopVmImportRequirement();
    expectCanonicalVectorReserveNamedArgsConformance(emitMode);
    expectCanonicalVectorReserveVmImportRequirement();
  }
}

inline void expectVectorIndexedRemovalOwnershipRejects(const std::string &emitMode) {
  expectVectorIndexedRemovalOwnershipReject(
      emitMode,
      "remove_at_drop",
      "remove_at requires drop-trivial vector element type until container drop semantics are implemented: Owned");
  expectVectorIndexedRemovalOwnershipReject(
      emitMode,
      "remove_swap_relocation",
      "remove_swap requires relocation-trivial vector element type until container move/reallocation semantics are "
      "implemented: Wrapper");
}
