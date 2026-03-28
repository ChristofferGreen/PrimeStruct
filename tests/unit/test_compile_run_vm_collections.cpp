#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

static void expectVmVectorCountCompatibilityTypeMismatchReject(const std::string &runCmd) {
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_vector_count_compatibility_type_mismatch_reject_out.txt")
                                  .string();
  const std::string captureCmd = runCmd + " > " + outPath + " 2>&1";
  CHECK(runCommand(captureCmd) != 0);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}


#include "test_compile_run_vm_collections_core_aliases.h"
#include "test_compile_run_vm_collections_vector_aliases_a.h"
#include "test_compile_run_vm_collections_vector_aliases_b.h"
#include "test_compile_run_vm_collections_wrapper_temporaries_a.h"
#include "test_compile_run_vm_collections_wrapper_temporaries_b.h"
#include "test_compile_run_vm_collections_wrapper_temporaries_c.h"
#include "test_compile_run_vm_collections_method_aliases_a.h"
#include "test_compile_run_vm_collections_method_aliases_b.h"
#include "test_compile_run_vm_collections_shim_maps_a.h"
#include "test_compile_run_vm_collections_shim_maps_b.h"
#include "test_compile_run_vm_collections_shim_maps_c.h"
#include "test_compile_run_vm_collections_array_and_wrapper_shadows.h"
#include "test_compile_run_vm_collections_map_wrapper_shadows.h"
#include "test_compile_run_vm_collections_map_vector_shadows.h"
#include "test_compile_run_vm_collections_vector_shadow_access.h"
#include "test_compile_run_vm_collections_vector_limits_a.h"
#include "test_compile_run_vm_collections_vector_limits_b.h"

TEST_SUITE_END();
