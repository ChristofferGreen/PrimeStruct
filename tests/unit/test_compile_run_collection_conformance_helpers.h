#pragma once

#include "test_compile_run_map_conformance_helpers.h"
#include "test_compile_run_vector_conformance_helpers.h"

inline void expectSharedMapConformanceHarness(const std::string &emitMode) {
  SUBCASE("stdlib") {
    expectMapHelperSurfaceConformance(emitMode, "/std/collections/*");
    expectMapExtendedConstructorConformance(emitMode, "/std/collections/*");
    expectMapOverwriteConformance(emitMode, "/std/collections/*");
    expectMapTryAtConformance(emitMode, "/std/collections/*", false);
    expectMapTryAtConformance(emitMode, "/std/collections/*", true);
  }

  SUBCASE("experimental") {
    expectMapHelperSurfaceConformance(emitMode, "/std/collections/experimental_map/*");
    expectMapExtendedConstructorConformance(emitMode, "/std/collections/experimental_map/*");
    expectMapOverwriteConformance(emitMode, "/std/collections/experimental_map/*");
    expectMapTryAtConformance(emitMode, "/std/collections/experimental_map/*", false);
    expectMapTryAtConformance(emitMode, "/std/collections/experimental_map/*", true);
    expectExperimentalMapAtMissingConformance(emitMode);
    expectExperimentalMapTryAtStringConformance(emitMode);
    if (emitMode != "exe") {
      expectExperimentalMapStringKeyReject(emitMode, "lookup_argv");
      expectExperimentalMapStringKeyReject(emitMode, "constructor_argv");
    }
  }
}

inline void expectSharedVectorConformanceHarness(const std::string &emitMode) {
  SUBCASE("stdlib") {
    expectVectorHelperSurfaceConformance(emitMode, "/std/collections/*");
    expectVectorExtendedConstructorConformance(emitMode, "/std/collections/*");
    expectVectorGrowthConformance(emitMode, "/std/collections/*");
    expectVectorShrinkRemoveConformance(emitMode, "/std/collections/*");
    expectVectorTypeMismatchReject(emitMode, "/std/collections/*");
    expectVectorPopTypeMismatchReject(emitMode, "/std/collections/*");
    expectVectorPushTypeMismatchReject(emitMode, "/std/collections/*");
    expectCanonicalVectorNamespaceConformance(emitMode);
    expectCanonicalVectorNamespaceNamedArgsConformance(emitMode);
    expectCanonicalVectorNamespaceTypeMismatchReject(emitMode);
    expectCanonicalVectorNamespaceImportRequirement(emitMode);
    expectCanonicalVectorCountNamedArgsConformance(emitMode);
    expectCanonicalVectorCountImportRequirement(emitMode);
    expectCanonicalVectorNamespaceCountShadow(emitMode);
    expectCanonicalVectorNamespacePushShadow(emitMode);
  }

  SUBCASE("experimental") {
    expectVectorHelperSurfaceConformance(emitMode, "/std/collections/experimental_vector/*");
    expectVectorExtendedConstructorConformance(emitMode, "/std/collections/experimental_vector/*");
    expectVectorGrowthConformance(emitMode, "/std/collections/experimental_vector/*");
    expectVectorShrinkRemoveConformance(emitMode, "/std/collections/experimental_vector/*");
    expectVectorTypeMismatchReject(emitMode, "/std/collections/experimental_vector/*");
    expectVectorPopTypeMismatchReject(emitMode, "/std/collections/experimental_vector/*");
    expectVectorPushTypeMismatchReject(emitMode, "/std/collections/experimental_vector/*");
  }
}

inline void expectExperimentalVectorRuntimeContracts(const std::string &emitMode) {
  expectVectorHelperRuntimeContract(emitMode, "/std/collections/experimental_vector/*", "pop_empty");
  expectVectorHelperRuntimeContract(emitMode, "/std/collections/experimental_vector/*", "remove_at_oob");
  expectVectorHelperRuntimeContract(emitMode, "/std/collections/experimental_vector/*", "remove_swap_oob");
}

inline void expectExperimentalVectorOwnershipRejects(const std::string &emitMode) {
  expectExperimentalVectorOwnershipReject(
      emitMode,
      "constructor",
      "clear requires drop-trivial vector element type until container drop semantics are implemented: Owned");
  expectExperimentalVectorOwnershipReject(
      emitMode,
      "push",
      "push requires relocation-trivial vector element type until container move/reallocation semantics are implemented: Owned");
  expectExperimentalVectorOwnershipReject(
      emitMode,
      "pop",
      "pop requires drop-trivial vector element type until container drop semantics are implemented: Owned");
  expectExperimentalVectorOwnershipReject(
      emitMode,
      "remove_swap",
      "remove_swap requires drop-trivial vector element type until container drop semantics are implemented: Owned");
}
